#pragma once
#include <cstdint>
#include <functional>

struct MotionRuntime {
    int64_t microseconds = 0;
    int attaches = 0;
    int writes = 0;
    int writes_after_cancel = 0;
    bool cancelled = false;
    std::function<void()> on_delay;
};
inline MotionRuntime motion_runtime;
