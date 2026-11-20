#include <cassert>
#include <limits>

#include "audio/wake_words/voice_command.h"
#include "local_stop_runtime.h"

int main() {
    assert(ResolveVoiceCommand("stop", .5f, true, .35f) == VoiceCommandAction::Stop);
    assert(ResolveVoiceCommand("stop", .2f, true, .35f) == VoiceCommandAction::Ignore);
    assert(ResolveVoiceCommand("stop", .8f, false, .35f) == VoiceCommandAction::Ignore);
    assert(ResolveVoiceCommand("wake", .1f, true, .35f) == VoiceCommandAction::Wake);
    assert(ResolveVoiceCommand("other", .9f, true, .35f) == VoiceCommandAction::Ignore);
    assert(ResolveVoiceCommand("stop", std::numeric_limits<float>::quiet_NaN(), true, .35f) ==
           VoiceCommandAction::Ignore);

    StopProtocol protocol;
    for (auto state :
         {kDeviceStateSpeaking, kDeviceStateListening, kDeviceStateConnecting, kDeviceStateIdle}) {
        Application app;
        protocol = StopProtocol();
        app.protocol_ = &protocol;
        app.state = state;
        auto previous_stops = Board::GetInstance().stops;
        app.HandleLocalStopEvent();
        assert(Board::GetInstance().stops == previous_stops + 1);
        assert(app.aborts == 1 && protocol.closes == 0 && protocol.open);
        assert(app.state == kDeviceStateListening && app.audio_service_.wake &&
               app.audio_service_.voice && app.listens == 1);
        assert(app.audio_service_.generation == 1 && app.audio_service_.send_packets == 0);
        assert(!app.play_popup_on_listening_ && !app.pending_listening_start_);
    }
    Application idle;
    idle.state = kDeviceStateIdle;
    idle.HandleLocalStopEvent();  // An idle device need not have a protocol object yet.
    assert(idle.aborts == 0 && idle.audio_service_.wake);
    Application reconnecting;
    protocol.open = false;
    reconnecting.protocol_ = &protocol;
    reconnecting.HandleLocalStopEvent();
    assert(reconnecting.state == kDeviceStateListening && reconnecting.reconnects == 1);
    assert(protocol.open && protocol.closes == 0 && reconnecting.audio_service_.voice);

    for (auto state : {kDeviceStateSpeaking, kDeviceStateListening, kDeviceStateIdle}) {
        Application app;
        app.protocol_ = &protocol;
        app.state = state;
        Board::GetInstance().moving = true;
        auto stops = Board::GetInstance().stops;
        app.HandleStopActionsEvent();
        assert(Board::GetInstance().stops == stops + 1);
        assert(app.state == state && app.aborts == 0 && app.audio_service_.generation == 0);
        assert(app.audio_service_.voice && app.audio_service_.wake && protocol.open);
        assert(app.audio_service_.send_packets == 3 && app.listens == 0);
        app.HandleStopActionsEvent();  // A repeated voice stop must not queue HOME while resting.
        assert(Board::GetInstance().stops == stops + 1);
    }
    Application configuring;
    configuring.state = kDeviceStateWifiConfiguring;
    auto previous_stops = Board::GetInstance().stops;
    configuring.HandleLocalStopEvent();
    configuring.HandleStopActionsEvent();
    assert(configuring.audio_service_.generation == 0 &&
           Board::GetInstance().stops == previous_stops);

    for (auto state : {kDeviceStateSpeaking, kDeviceStateListening}) {
        Application app;
        app.protocol_ = &protocol;
        app.state = state;
        app.HandleWakeWordDetectedEvent();
        assert(app.aborts == 0 && app.wake_invokes == 0 && app.state == state);
        assert(app.audio_service_.wake);  // Wake phrase is not an active-turn stop command.
    }
    Application waking;
    waking.protocol_ = &protocol;
    waking.state = kDeviceStateIdle;
    waking.HandleWakeWordDetectedEvent();
    assert(waking.wake_invokes == 1 && waking.aborts == 0);

    // A higher-priority main task can rearm detection before the callback
    // returns. The production recognizer must not erase its cleared buffer.
    int16_t samples[8] = {};
    for (const char* action : {"wake", "stop"}) {
        CustomWakeWord detector;
        detector.commands_.push_back({"test", "test", action});
        int calls = 0;
        detector.wake_word_detected_callback_ = [&](const std::string&) {
            ++calls;
            detector.running_ = true;
        };
        detector.stop_command_detected_callback_ = [&] {
            ++calls;
            detector.running_ = true;
        };
        detector.FeedSamples(samples, 8, true);
        assert(calls == 1 && detector.input_buffer_.empty() && detector.model.clean_count == 1);
    }
    CustomWakeWord invalid;
    invalid.commands_.push_back({"test", "test", "stop"});
    invalid.model.result.command_id[0] = 100;
    invalid.FeedSamples(samples, 8, true);
    assert(invalid.input_buffer_.empty());
    CustomWakeWord low_probability;
    low_probability.commands_.push_back({"test", "test", "stop"});
    low_probability.model.result.prob[0] = .2f;
    int stopped = 0;
    low_probability.stop_command_detected_callback_ = [&] { ++stopped; };
    low_probability.FeedSamples(samples, 8, true);
    assert(stopped == 0 && low_probability.running_);

    // Recorded probabilities from the physical robot now pass the configured
    // 10% threshold; a below-threshold result still leaves detection running.
    for (float probability : {.171861f, .160266f, .09f}) {
        CustomWakeWord detector;
        detector.stop_threshold_ = .1f;
        detector.commands_.push_back({"ting xia", "停下", "stop"});
        detector.model.result.prob[0] = probability;
        int calls = 0;
        detector.stop_command_detected_callback_ = [&] { ++calls; };
        detector.FeedSamples(samples, 8, true);
        assert(calls == (probability >= .1f ? 1 : 0));
    }
}
