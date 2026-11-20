#pragma once
#include "FreeRTOS.h"
#include "runtime.h"
inline void vTaskDelay(TickType_t ticks) {
    motion_runtime.microseconds += ticks * 10000;
    if (motion_runtime.on_delay)
        motion_runtime.on_delay();
}
