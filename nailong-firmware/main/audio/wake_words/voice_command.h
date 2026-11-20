#pragma once

#include <string_view>

enum class VoiceCommandAction { Ignore, Wake, Stop };

inline VoiceCommandAction ResolveVoiceCommand(std::string_view action, float probability,
                                              bool stop_enabled, float stop_threshold) {
    if (action == "wake")
        return VoiceCommandAction::Wake;
    if (stop_enabled && action == "stop" && probability >= stop_threshold) {
        return VoiceCommandAction::Stop;
    }
    return VoiceCommandAction::Ignore;
}
