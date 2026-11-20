#pragma once

#include <atomic>
#include <cassert>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include "audio/wake_words/voice_command.h"
#include "device_state.h"

#define ESP_LOGI(...) ((void)0)
#define ESP_LOGD(...) ((void)0)
#define CONFIG_ENABLE_LOCAL_STOP_COMMAND 1
constexpr int kAbortReasonNone = 0;
constexpr int kAbortReasonWakeWordDetected = 1;
namespace Lang::Sounds {
inline std::string OGG_POPUP;
}

struct Board {
    int stops = 0;
    bool moving = true;
    static Board& GetInstance() {
        static Board board;
        return board;
    }
    bool HasActiveActions() { return moving; }
    void StopActions() {
        ++stops;
        moving = false;
    }
};
struct StopAudio {
    bool voice = true;
    bool wake = false;
    int generation = 0;
    int send_packets = 3;
    std::string word = "你好奶龙";
    void EnableVoiceProcessing(bool enable) { voice = enable; }
    void EnableWakeWordDetection(bool enable) { wake = enable; }
    void ResetDecoder() { ++generation; }
    bool PopPacketFromSendQueue() {
        if (!send_packets)
            return false;
        --send_packets;
        return true;
    }
    const std::string& GetLastWakeWord() { return word; }
    void PlaySound(const std::string&) {}
};
struct StopProtocol {
    bool open = true;
    int closes = 0;
    bool IsAudioChannelOpened() { return open; }
    void CloseAudioChannel() {
        open = false;
        ++closes;
    }
    void SendStartListening(int) {}
};
struct Application {
    DeviceState state = kDeviceStateSpeaking;
    StopAudio audio_service_;
    StopProtocol* protocol_ = nullptr;
    bool play_popup_on_listening_ = true;
    bool pending_listening_start_ = true;
    int aborts = 0;
    int wake_invokes = 0;
    int listens = 0;
    int reconnects = 0;
    DeviceState GetDeviceState() { return state; }
    void SetDeviceState(DeviceState next) {
        if (next == kDeviceStateConnecting)
            assert(state == kDeviceStateIdle || state == kDeviceStateConnecting);
        state = next;
    }
    void AbortSpeaking(int) { ++aborts; }
    void BeginWakeWordInvoke(const std::string&) { ++wake_invokes; }
    int GetDefaultListeningMode() { return 0; }
    void SetListeningMode(int) { state = kDeviceStateListening; }
    void StartListeningAudio() {
        ++listens;
        audio_service_.voice = true;
    }
    void Schedule(std::function<void()> callback) { callback(); }
    void ContinueOpenAudioChannel(int mode) {
        ++reconnects;
        protocol_->open = true;
        SetListeningMode(mode);
        StartListeningAudio();
    }
    void HandleLocalStopEvent();
    void HandleStopActionsEvent();
    void HandleWakeWordDetectedEvent();
};

constexpr int ESP_MN_STATE_DETECTED = 1;
constexpr int ESP_MN_STATE_TIMEOUT = 2;
using esp_mn_state_t = int;
struct esp_mn_results_t {
    int num = 1;
    int command_id[1] = {1};
    const char* string = "test-command";
    float prob[1] = {.8f};
};
struct StopModel {
    esp_mn_results_t result;
    int clean_count = 0;
    int get_samp_chunksize(void*) { return 4; }
    int detect(void*, const int16_t*) { return ESP_MN_STATE_DETECTED; }
    esp_mn_results_t* get_results(void*) { return &result; }
    void clean(void*) { ++clean_count; }
};
struct StopCodec {
    int input_channels() { return 1; }
};
struct CustomWakeWord {
    struct Command {
        std::string command, text, action;
    };
    StopModel model;
    StopModel* multinet_ = &model;
    void* multinet_model_data_ = this;
    StopCodec codec;
    StopCodec* codec_ = &codec;
    std::atomic<bool> running_{true};
    std::vector<int16_t> input_buffer_;
    std::mutex input_buffer_mutex_;
    std::vector<Command> commands_;
    std::string last_detected_wake_word_;
    float stop_threshold_ = .35f;
    std::function<void(const std::string&)> wake_word_detected_callback_;
    std::function<void()> stop_command_detected_callback_;
    void FeedSamples(const int16_t* data, size_t samples, bool mono);
};
