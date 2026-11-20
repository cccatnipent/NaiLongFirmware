#pragma once

#include "nailong_scene_state.h"
#include "otto_emoji_display.h"

class NailongDisplay : public OttoEmojiDisplay {
public:
    using OttoEmojiDisplay::OttoEmojiDisplay;
    ~NailongDisplay() override;
    void SetupUI() override;
    void SetStatus(const char* status) override;
    void SetEmotion(const char* emotion) override;
    void SetTheme(Theme* theme) override;
    void SetLaughing(bool laughing);

private:
    NailongSceneState scene_;
    lv_obj_t* nailong_image_ = nullptr;
    lv_timer_t* scene_timer_ = nullptr;
    lv_image_dsc_t image_desc_ = {};
    uint8_t* pixels_ = nullptr;
    int current_image_ = -1;
    bool assets_available_ = false;

    bool CheckImagesLocked();
    void RefreshImageLocked();
};
