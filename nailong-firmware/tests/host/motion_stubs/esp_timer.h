#pragma once
#include "runtime.h"
inline int64_t esp_timer_get_time() { return motion_runtime.microseconds; }
