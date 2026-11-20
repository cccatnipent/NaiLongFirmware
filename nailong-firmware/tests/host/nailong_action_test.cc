#include <cassert>

#include "nailong_actions.h"

static void WaitFor(const std::function<bool()>& done) {
    auto deadline = esp_timer_get_time() + 2000000;
    while (!done() && esp_timer_get_time() < deadline) {
        Application::GetInstance().Pump();
        vTaskDelay(1);
    }
    assert(done());
}

int main() {
    auto& app = Application::GetInstance();
    auto& audio = app.audio;
    NailongDisplay display;
    NailongActions actions(&display);
    app.observer = [&](DeviceState state) { actions.OnDeviceStateChanged(state); };
    audio.on_play = [&] {
        assert(app.GetDeviceState() == kDeviceStateSpeaking);
        assert(display.laughing);
    };
    std::vector<std::string> replies;
    auto call = [&] {
        McpServer::GetInstance().laugh(PropertyList(),
                                       [&](ReturnValue result) { replies.push_back(result); });
    };

    // Reproduce the device log: cloud sends TTS start, then tools/call, and
    // withholds TTS stop until its tool result. No new speaking notification.
    app.state = kDeviceStateSpeaking;
    call();
    WaitFor([&] { return audio.plays == 1; });
    assert(replies.empty());
    audio.idle = true;
    WaitFor([&] { return replies.size() == 1; });
    assert(replies.back().find("已播放完毕") != std::string::npos);
    assert(app.GetDeviceState() == kDeviceStateSpeaking);
    assert(!display.laughing);

    // Existing cloud speech must drain before local playback; duplicate calls
    // are rejected without starting another clip or completing the first one.
    audio.idle = false;
    call();
    for (int i = 0; i < 100; ++i) {
        app.Pump();
        vTaskDelay(1);
    }
    assert(audio.plays == 1 && replies.size() == 1);
    audio.idle = true;
    WaitFor([&] { return audio.plays == 2; });
    call();
    assert(replies.size() == 2 && replies.back().find("正在播放") != std::string::npos);
    audio.idle = true;
    WaitFor([&] { return replies.size() == 3; });
    assert(replies.back().find("已播放完毕") != std::string::npos);
    app.SetDeviceState(kDeviceStateListening);  // Cloud continuation eventually sends stop.
    app.Pump();

    // A call while listening still enters speaking and waits for the actual
    // state handler to reset the decoder before acknowledging startup.
    call();
    WaitFor([&] { return audio.plays == 3; });
    assert(app.GetDeviceState() == kDeviceStateSpeaking);
    audio.idle = true;
    WaitFor([&] { return replies.size() == 4; });
    assert(app.GetDeviceState() == kDeviceStateListening && !display.laughing);
    app.Pump();

    // A button-driven state change cancels playback and is never undone by
    // the local completion callback restoring the old cloud speaking state.
    app.SetDeviceState(kDeviceStateSpeaking);
    app.Pump();
    call();
    WaitFor([&] { return audio.plays == 4; });
    app.SetDeviceState(kDeviceStateListening);
    app.Pump();
    WaitFor([&] { return replies.size() == 5; });
    assert(replies.back().find("已停止") != std::string::npos);
    assert(app.GetDeviceState() == kDeviceStateListening && !display.laughing);

    auto combo = [&](int steps = 5, int speed = 700, int direction = 1) {
        McpServer::GetInstance().laugh_walk(
            PropertyList({Property("steps", kPropertyTypeInteger, steps, 1, 7),
                          Property("speed", kPropertyTypeInteger, speed, 300, 1500),
                          Property("direction", kPropertyTypeInteger, direction, -1, 1)}),
            [&](ReturnValue result) { replies.push_back(result); });
    };
    // A busy servo controller rejects the whole combined operation, rather
    // than queuing a walk that would only start after the laugh ends.
    app.SetDeviceState(kDeviceStateSpeaking);
    app.Pump();
    host_motion.reject = true;
    combo();
    WaitFor([&] { return replies.size() == 6; });
    assert(audio.plays == 4 && host_motion.starts == 0 && !display.laughing);
    assert(replies.back().find("均未开始") != std::string::npos);
    host_motion.reject = false;

    host_motion.on_start = [&] { assert(app.GetDeviceState() == kDeviceStateSpeaking); };
    audio.on_play = [&] {
        assert(display.laughing);
        assert(NailongMotion::GetStatus() == NailongMotion::Status::Running);
    };
    combo();
    WaitFor([&] { return audio.plays == 5; });
    assert(host_motion.starts == 1 && replies.size() == 6);
    audio.idle = true;
    for (int i = 0; i < 100; ++i) {
        app.Pump();
        vTaskDelay(1);
    }
    assert(replies.size() == 6);  // Audio drain alone must not finish the tool.
    host_motion.status = NailongMotion::Status::Completed;
    WaitFor([&] { return replies.size() == 7; });
    assert(replies.back().find("边笑边走") != std::string::npos);
    assert(!display.laughing && app.GetDeviceState() == kDeviceStateSpeaking);

    combo();
    WaitFor([&] { return audio.plays == 6; });
    app.SetDeviceState(kDeviceStateListening);
    app.Pump();
    WaitFor([&] { return replies.size() == 8; });
    assert(host_motion.cancellations > 0);
    assert(NailongMotion::GetStatus() == NailongMotion::Status::Cancelled);
    assert(replies.back().find("已停止") != std::string::npos);
    assert(app.GetDeviceState() == kDeviceStateListening && !display.laughing);

    combo(5, 700, 0);
    combo(7, 1500, 1);
    assert(replies.size() == 10 && audio.plays == 6 && host_motion.starts == 2);
    assert(!NailongMotion::ValidRequest(0, 700, 1));
    assert(!NailongMotion::ValidRequest(8, 300, 1));
    assert(!NailongMotion::ValidRequest(3, 299, 1));
    assert(NailongMotion::ValidRequest(3, 700, -1));

    // Voice stop cancels the gait, while the same local laugh keeps playing.
    app.SetDeviceState(kDeviceStateSpeaking);
    app.Pump();
    combo();
    WaitFor([&] { return audio.plays == 7; });
    const auto generation = audio.GetPlaybackGeneration();
    host_motion.status = NailongMotion::Status::Cancelled;
    for (int i = 0; i < 100; ++i) {
        app.Pump();
        vTaskDelay(1);
    }
    assert(replies.size() == 10 && display.laughing && !audio.idle);
    assert(audio.GetPlaybackGeneration() == generation);
    audio.idle = true;
    WaitFor([&] { return replies.size() == 11; });
    assert(replies.back().find("笑声已播放完毕，行走已停止") != std::string::npos);
    assert(app.GetDeviceState() == kDeviceStateSpeaking && !display.laughing);
    StopHostWorkers();
}
