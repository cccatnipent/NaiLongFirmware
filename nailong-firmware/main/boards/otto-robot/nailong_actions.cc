#include "nailong_actions.h"

#include <esp_log.h>
#include <esp_timer.h>

#include <memory>
#include <string>

#include "application.h"
#include "assets.h"
#include "nailong_display.h"
#include "nailong_motion.h"

namespace {
bool CanLaughInState(DeviceState state) {
    // The server opens a TTS turn before calling tools and waits for their result
    // before sending TTS stop. Speaking therefore does not imply queued audio.
    return state == kDeviceStateIdle || state == kDeviceStateListening ||
           state == kDeviceStateSpeaking;
}
}  // namespace

NailongActions::NailongActions(NailongDisplay* display) : display_(display) {
    queue_ = xQueueCreate(1, sizeof(McpTool::Completion*));
    if (queue_ && xTaskCreate([](void* arg) { static_cast<NailongActions*>(arg)->Run(); },
                              "nailong_laugh", 6144, this, 3, &task_) != pdPASS) {
        task_ = nullptr;
    }
    McpServer::GetInstance().AddDeferredTool(
        "self.otto.laugh",
        "播放本地奶龙大笑并显示大笑图片，约5秒。用户要求笑一个、笑一下、来段奶龙笑声时调用。"
        "听不懂或不理解用户的意思、需要用户重新说明时也应调用，然后简短询问。"
        "用户要求边笑边走时改用self.otto.laugh_walk，不要把笑与行走分开调用。"
        "工具完成后笑声已经播放，不要用语音合成重复哈哈哈。",
        PropertyList(), [this](const PropertyList&, McpTool::Completion completion) {
            QueueLaugh(std::move(completion));
        });
    McpServer::GetInstance().AddDeferredTool(
        "self.otto.laugh_walk",
        "同时播放5秒本地奶龙笑声并行走，边笑边往前走/往后走时必须用此组合工具，"
        "不要先调用laugh再调用action。steps为步数，speed为每步毫秒，direction为1前进或-1后退；"
        "steps乘speed不能超过4500。设备本地并行执行，等两项结束后返回。",
        PropertyList({Property("steps", kPropertyTypeInteger, 5, 1, 7),
                      Property("speed", kPropertyTypeInteger, 700, 300, 1500),
                      Property("direction", kPropertyTypeInteger, 1, -1, 1)}),
        [this](const PropertyList& properties, McpTool::Completion completion) {
            int steps = properties["steps"].value<int>();
            int speed = properties["speed"].value<int>();
            int direction = properties["direction"].value<int>();
            if (!NailongMotion::ValidRequest(steps, speed, direction)) {
                completion(std::string("未执行：方向必须为1或-1，步数乘每步毫秒不能超过4500。"));
                return;
            }
            QueueLaugh(std::move(completion), steps, speed, direction);
        });
}

void NailongActions::QueueLaugh(McpTool::Completion completion, int steps, int speed,
                                int direction) {
    auto request = std::make_unique<McpTool::Completion>(std::move(completion));
    if (!task_ || busy_.exchange(true)) {
        (*request)(std::string("未开始：大笑任务不可用或正在播放，请勿重复调用。"));
        return;
    }
    walk_steps_ = steps;
    walk_speed_ = speed;
    walk_direction_ = direction;
    walk_started_ = false;
    motion_rejected_ = false;
    auto* raw = request.get();
    if (xQueueSend(queue_, &raw, 0) != pdTRUE) {
        busy_ = false;
        (*request)(std::string("未开始：大笑任务忙。"));
        return;
    }
    request.release();
}

void NailongActions::OnDeviceStateChanged(DeviceState state) {
    auto& audio = Application::GetInstance().GetAudioService();
    if (phase_ == Phase::Preparing && state == kDeviceStateSpeaking) {
        generation_ = audio.GetPlaybackGeneration();
        if (walk_steps_ > 0) {
            walk_started_ = NailongMotion::TryStart(walk_steps_, walk_speed_, walk_direction_);
            if (!walk_started_) {
                motion_rejected_ = true;
                phase_ = Phase::Finishing;
                xTaskNotifyGive(task_);
                return;
            }
        }
        phase_ = Phase::Playing;
        xTaskNotifyGive(task_);
    } else if (phase_ == Phase::Playing && state != kDeviceStateSpeaking) {
        audio.ResetDecoder();
        if (walk_started_)
            NailongMotion::Cancel();
    }
}

void NailongActions::Finish(McpTool::Completion* completion, const char* result, bool clear_audio) {
    ESP_LOGI("NailongActions", "%s", result);
    ++ticket_;  // Invalidate a start callback if its main-task acknowledgement timed out.
    phase_ = Phase::Finishing;
    Application::GetInstance().Schedule(
        [this, completion, message = std::string(result), clear_audio]() {
            std::unique_ptr<McpTool::Completion> request(completion);
            auto& app = Application::GetInstance();
            if (started_ && app.GetDeviceState() == kDeviceStateSpeaking &&
                app.GetAudioService().GetPlaybackGeneration() == generation_) {
                if (clear_audio) {
                    app.GetAudioService().ResetDecoder();
                }
                // A cloud TTS turn must remain open until the tool result has
                // been delivered. Its own TTS stop will restore listening.
                if (previous_state_ != kDeviceStateSpeaking) {
                    app.SetDeviceState(previous_state_);
                }
            }
            display_->SetLaughing(false);
            if (walk_started_ && clear_audio)
                NailongMotion::Cancel();
            walk_started_ = false;
            started_ = false;
            phase_ = Phase::Idle;
            busy_ = false;
            (*request)(message);
        });
}

void NailongActions::Run() {
    for (;;) {
        McpTool::Completion* completion = nullptr;
        if (xQueueReceive(queue_, &completion, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        auto& app = Application::GetInstance();
        auto& audio = app.GetAudioService();
        void* ptr = nullptr;
        size_t size = 0;
        if (!Assets::GetInstance().GetAssetData("nl_laugh.ogg", ptr, size) || size > 256 * 1024) {
            Finish(completion, "未播放：缺少或无效的奶龙音频资源。", false);
            continue;
        }
        // Keep the small local clip independent of any later resource-partition refresh.
        std::string sound(static_cast<const char*>(ptr), size);
        ESP_LOGI("NailongActions", "Waiting for audio drain (state=%d)", app.GetDeviceState());
        int64_t deadline = esp_timer_get_time() + 15000000;
        while (esp_timer_get_time() < deadline &&
               (!CanLaughInState(app.GetDeviceState()) || !audio.IsPlaybackIdle())) {
            vTaskDelay(pdMS_TO_TICKS(20));
        }
        if (!audio.IsPlaybackIdle() || !CanLaughInState(app.GetDeviceState())) {
            Finish(completion, "未播放：当前设备正在执行其他音频或系统任务。", false);
            continue;
        }
        ulTaskNotifyTake(pdTRUE, 0);
        unsigned ticket = ++ticket_;
        phase_ = Phase::Preparing;
        app.Schedule([this, ticket]() {
            if (ticket != ticket_ || phase_ != Phase::Preparing) {
                return;
            }
            auto& app = Application::GetInstance();
            previous_state_ = app.GetDeviceState();
            if (!CanLaughInState(previous_state_) || !app.GetAudioService().IsPlaybackIdle()) {
                phase_ = Phase::Finishing;
                xTaskNotifyGive(task_);
                return;
            }
            display_->SetLaughing(true);
            if (previous_state_ == kDeviceStateSpeaking) {
                // SetDeviceState(Speaking) would be a no-op: no state-change
                // notification would arrive to acknowledge playback startup.
                started_ = true;
                OnDeviceStateChanged(kDeviceStateSpeaking);
                return;
            }
            started_ = app.SetDeviceState(kDeviceStateSpeaking);
            if (!started_) {
                phase_ = Phase::Finishing;
                xTaskNotifyGive(task_);
            }
        });
        // Board notification runs after the speaking state has reset the decoder and muted input.
        if (!ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(5000)) || phase_ != Phase::Playing) {
            Finish(completion,
                   motion_rejected_ ? "未执行：行走控制器正忙，笑声和行走均未开始。"
                                    : "未播放：本地音频启动被打断或超时。",
                   true);
            continue;
        }
        ESP_LOGI("NailongActions", "Playing local laugh: %u bytes, generation=%lu",
                 static_cast<unsigned>(sound.size()), static_cast<unsigned long>(generation_));
        bool accepted = audio.PlaySoundCancelable(sound, generation_);
        deadline = esp_timer_get_time() + 15000000;
        while (!audio.IsPlaybackIdle() && esp_timer_get_time() < deadline &&
               audio.GetPlaybackGeneration() == generation_) {
            vTaskDelay(pdMS_TO_TICKS(20));
        }
        bool completed =
            accepted && audio.IsPlaybackIdle() && audio.GetPlaybackGeneration() == generation_;
        if (walk_started_) {
            const bool audio_completed = completed;
            if (!completed)
                NailongMotion::Cancel();
            deadline = esp_timer_get_time() + 5000000;
            while (NailongMotion::Active(NailongMotion::GetStatus()) &&
                   esp_timer_get_time() < deadline) {
                if (audio.GetPlaybackGeneration() != generation_) {
                    completed = false;
                    NailongMotion::Cancel();
                }
                vTaskDelay(pdMS_TO_TICKS(20));
            }
            completed = completed && NailongMotion::GetStatus() == NailongMotion::Status::Completed;
            Finish(completion,
                   completed
                       ? "奶龙已边笑边走，两项均执行完毕，不要重复调用行走或合成笑声。"
                       : (audio_completed ? "奶龙笑声已播放完毕，行走已停止，不要重新启动动作。"
                                          : "奶龙笑声与行走组合已停止。"),
                   !audio_completed);
            continue;
        }
        Finish(
            completion,
            completed ? "奶龙大笑已播放完毕，请继续对话，不要重复合成笑声。" : "奶龙大笑已停止。",
            !completed);
    }
}
