#pragma once

// Independent of LVGL and FreeRTOS so reply selection and display priority can be tested on host.
class NailongSceneState {
public:
    enum class Status { Standby, Listening, Speaking, System };

    void SetStatus(Status status, unsigned random) {
        if (status == Status::Standby) {
            ready_ = true;
        }
        system_ = status == Status::System || !ready_;
        bool speaking = status == Status::Speaking;
        if (speaking && !speaking_ && laughing_) {
            reply_image_ = 0;
        }
        if (speaking && !speaking_ && !laughing_ && ready_) {
            int slot = random % (last_slot_ < 0 ? 7 : 6);
            if (last_slot_ >= 0 && slot >= last_slot_) {
                ++slot;
            }
            last_slot_ = slot;
            reply_image_ = slot + 2;
        }
        if (!speaking && speaking_ && !system_) {
            draining_ = true;
        }
        if (system_) {
            draining_ = false;
        }
        speaking_ = speaking;
    }

    void SetLaughing(bool laughing) { laughing_ = laughing; }
    bool ready() const { return ready_; }

    int Image(bool playback_idle) {
        if (draining_ && playback_idle) {
            draining_ = false;
        }
        if (!ready_ || system_) {
            return -1;
        }
        if (laughing_) {
            return 1;
        }
        return speaking_ || draining_ ? reply_image_ : 0;
    }

private:
    bool ready_ = false;
    bool system_ = true;
    bool speaking_ = false;
    bool draining_ = false;
    bool laughing_ = false;
    int last_slot_ = -1;
    int reply_image_ = 0;
};
