#pragma once

// Host fakes for the board action worker. Production nailong_actions.cc is
// compiled unchanged in a temporary directory against these interfaces.
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "device_state.h"
#include "nailong_motion.h"

constexpr int pdTRUE = 1;
constexpr int pdPASS = 1;
constexpr uint32_t portMAX_DELAY = UINT32_MAX;
inline uint32_t pdMS_TO_TICKS(uint32_t ms) { return ms; }
struct StopWorker {};
struct HostQueue {
    std::mutex mutex;
    std::condition_variable cv;
    void* item = nullptr;
    bool stopping = false;
};
struct HostTask {
    std::thread thread;
    std::mutex mutex;
    std::condition_variable cv;
    unsigned notifications = 0;
};
using QueueHandle_t = HostQueue*;
using TaskHandle_t = HostTask*;
inline thread_local HostTask* current_task = nullptr;
inline std::vector<HostQueue*> host_queues;
inline std::vector<HostTask*> host_tasks;
inline QueueHandle_t xQueueCreate(unsigned, unsigned) {
    auto* queue = new HostQueue;
    host_queues.push_back(queue);
    return queue;
}
inline int xQueueSend(QueueHandle_t queue, void* item, uint32_t) {
    std::lock_guard lock(queue->mutex);
    if (queue->item) {
        return 0;
    }
    queue->item = *static_cast<void**>(item);
    queue->cv.notify_one();
    return pdTRUE;
}
inline int xQueueReceive(QueueHandle_t queue, void* item, uint32_t) {
    std::unique_lock lock(queue->mutex);
    queue->cv.wait(lock, [&] { return queue->item || queue->stopping; });
    if (queue->stopping) {
        throw StopWorker{};
    }
    *static_cast<void**>(item) = queue->item;
    queue->item = nullptr;
    return pdTRUE;
}
inline int xTaskCreate(void (*run)(void*), const char*, unsigned, void* arg, unsigned,
                       TaskHandle_t* output) {
    auto* task = new HostTask;
    *output = task;
    host_tasks.push_back(task);
    task->thread = std::thread([=] {
        current_task = task;
        try {
            run(arg);
        } catch (const StopWorker&) {
        }
    });
    return pdPASS;
}
inline void vTaskDelay(uint32_t ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
inline void xTaskNotifyGive(TaskHandle_t task) {
    std::lock_guard lock(task->mutex);
    ++task->notifications;
    task->cv.notify_one();
}
inline unsigned ulTaskNotifyTake(int, uint32_t timeout) {
    std::unique_lock lock(current_task->mutex);
    current_task->cv.wait_for(lock, std::chrono::milliseconds(timeout),
                              [] { return current_task->notifications > 0; });
    auto result = current_task->notifications;
    current_task->notifications = 0;
    return result;
}
inline void StopHostWorkers() {
    for (auto* queue : host_queues) {
        std::lock_guard lock(queue->mutex);
        queue->stopping = true;
        queue->cv.notify_all();
    }
    for (auto* task : host_tasks) {
        task->thread.join();
        delete task;
    }
    for (auto* queue : host_queues) {
        delete queue;
    }
}
inline int64_t esp_timer_get_time() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}
#define ESP_LOGI(...) ((void)0)

using ReturnValue = std::string;
constexpr int kPropertyTypeInteger = 0;
struct Property {
    std::string name;
    int number;
    Property(const char* name, int, int number, int, int) : name(name), number(number) {}
    template <typename T>
    T value() const {
        return static_cast<T>(number);
    }
};
struct PropertyList {
    std::vector<Property> properties;
    PropertyList() = default;
    PropertyList(std::initializer_list<Property> values) : properties(values) {}
    const Property& operator[](const char* name) const {
        for (const auto& property : properties) {
            if (property.name == name)
                return property;
        }
        throw std::out_of_range(name);
    }
};
struct McpTool {
    using Completion = std::function<void(ReturnValue)>;
    using DeferredCallback = std::function<void(const PropertyList&, Completion)>;
};
struct McpServer {
    McpTool::DeferredCallback laugh;
    McpTool::DeferredCallback laugh_walk;
    static McpServer& GetInstance() {
        static McpServer instance;
        return instance;
    }
    void AddDeferredTool(const char* name, const char*, PropertyList,
                         McpTool::DeferredCallback callback) {
        if (std::string(name) == "self.otto.laugh_walk")
            laugh_walk = std::move(callback);
        else
            laugh = std::move(callback);
    }
};
struct HostMotion {
    std::atomic<NailongMotion::Status> status{NailongMotion::Status::Idle};
    bool reject = false;
    unsigned starts = 0;
    unsigned cancellations = 0;
    std::function<void()> on_start;
};
inline HostMotion host_motion;
namespace NailongMotion {
inline bool TryStart(int steps, int speed, int direction) {
    if (host_motion.reject || Active(host_motion.status) || !ValidRequest(steps, speed, direction))
        return false;
    host_motion.status = Status::Running;
    ++host_motion.starts;
    if (host_motion.on_start)
        host_motion.on_start();
    return true;
}
inline Status GetStatus() { return host_motion.status; }
inline void Cancel() {
    ++host_motion.cancellations;
    if (Active(host_motion.status))
        host_motion.status = Status::Cancelled;
}
}  // namespace NailongMotion
struct NailongDisplay {
    bool laughing = false;
    void SetLaughing(bool value) { laughing = value; }
};
struct Assets {
    std::string clip = "test-local-clip";
    static Assets& GetInstance() {
        static Assets instance;
        return instance;
    }
    bool GetAssetData(const char*, void*& ptr, size_t& size) {
        ptr = clip.data();
        size = clip.size();
        return true;
    }
};
struct HostAudio {
    std::atomic<bool> idle{true};
    std::atomic<uint32_t> generation{0};
    std::atomic<unsigned> plays{0};
    std::function<void()> on_play;
    bool IsPlaybackIdle() { return idle; }
    uint32_t GetPlaybackGeneration() { return generation; }
    void ResetDecoder() {
        ++generation;
        idle = true;
    }
    bool PlaySoundCancelable(const std::string&, uint32_t expected_generation) {
        if (expected_generation != generation) {
            return false;
        }
        on_play();
        idle = false;
        ++plays;
        return true;
    }
};
struct Application {
    HostAudio audio;
    std::atomic<DeviceState> state{kDeviceStateIdle};
    std::function<void(DeviceState)> observer;
    std::mutex mutex;
    std::vector<std::function<void()>> scheduled;
    static Application& GetInstance() {
        static Application instance;
        return instance;
    }
    HostAudio& GetAudioService() { return audio; }
    DeviceState GetDeviceState() { return state; }
    void Schedule(std::function<void()> callback) {
        std::lock_guard lock(mutex);
        scheduled.push_back(std::move(callback));
    }
    bool SetDeviceState(DeviceState value) {
        if (state == value) {
            return true;  // Real state machine: same-state transitions never notify.
        }
        state = value;
        Schedule([this, value] {
            if (value == kDeviceStateSpeaking) {
                audio.ResetDecoder();
            }
            observer(value);  // After audio/UI handling, as in the real application.
        });
        return true;
    }
    void Pump() {
        std::vector<std::function<void()>> tasks;
        {
            std::lock_guard lock(mutex);
            tasks.swap(scheduled);
        }
        for (auto& task : tasks) {
            task();
        }
    }
};
