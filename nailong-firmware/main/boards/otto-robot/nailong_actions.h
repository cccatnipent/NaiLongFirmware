#pragma once

#include <atomic>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "device_state.h"
#include "mcp_server.h"

class NailongDisplay;

class NailongActions {
public:
    explicit NailongActions(NailongDisplay* display);
    void OnDeviceStateChanged(DeviceState state);

private:
    enum class Phase { Idle, Preparing, Playing, Finishing };
    NailongDisplay* display_;
    QueueHandle_t queue_ = nullptr;
    TaskHandle_t task_ = nullptr;
    std::atomic<bool> busy_{false};
    std::atomic<Phase> phase_{Phase::Idle};
    std::atomic<unsigned> ticket_{0};
    uint32_t generation_ = 0;
    DeviceState previous_state_ = kDeviceStateIdle;
    bool started_ = false;
    int walk_steps_ = 0;
    int walk_speed_ = 700;
    int walk_direction_ = 1;
    bool walk_started_ = false;
    bool motion_rejected_ = false;

    void QueueLaugh(McpTool::Completion completion, int steps = 0, int speed = 700,
                    int direction = 1);
    void Run();
    void Finish(McpTool::Completion* completion, const char* result, bool clear_audio);
};
