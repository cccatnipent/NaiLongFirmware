#include "nailong_display.h"

#include <esp_heap_caps.h>
#include <esp_random.h>
#include <src/misc/cache/instance/lv_image_cache.h>

#include <cstring>

#include "application.h"
#include "assets.h"
#include "assets/lang_config.h"

namespace {
constexpr const char* kImages[] = {"nl_idle.raw",   "nl_laugh.raw",  "nl_reply1.raw",
                                   "nl_reply2.raw", "nl_reply3.raw", "nl_reply4.raw",
                                   "nl_reply5.raw", "nl_reply6.raw", "nl_reply7.raw"};
constexpr size_t kImageBytes = 240 * 240 * 2;
}  // namespace

NailongDisplay::~NailongDisplay() {
    DisplayLockGuard lock(this);
    if (scene_timer_) {
        lv_timer_delete(scene_timer_);
    }
    if (nailong_image_) {
        lv_image_cache_drop(&image_desc_);
        lv_obj_delete(nailong_image_);
    }
    heap_caps_free(pixels_);
}

void NailongDisplay::SetupUI() {
    if (setup_ui_called_) {
        return;
    }
    OttoEmojiDisplay::SetupUI();
    DisplayLockGuard lock(this);
    pixels_ =
        static_cast<uint8_t*>(heap_caps_malloc(kImageBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!pixels_) {
        ESP_LOGE("NailongDisplay", "Unable to allocate the image buffer in PSRAM");
        return;
    }
    nailong_image_ = lv_image_create(emoji_box_);
    lv_obj_center(nailong_image_);
    lv_obj_add_flag(nailong_image_, LV_OBJ_FLAG_HIDDEN);
    scene_timer_ = lv_timer_create(
        [](lv_timer_t* timer) {
            auto* self = static_cast<NailongDisplay*>(lv_timer_get_user_data(timer));
            self->RefreshImageLocked();
        },
        50, this);
}

bool NailongDisplay::CheckImagesLocked() {
    if (!pixels_) {
        return false;
    }
    for (const char* name : kImages) {
        void* ptr = nullptr;
        size_t size = 0;
        if (!Assets::GetInstance().GetAssetData(name, ptr, size) || size != kImageBytes) {
            ESP_LOGE("NailongDisplay", "Missing/invalid image: %s", name);
            return false;
        }
    }
    return true;
}

void NailongDisplay::SetStatus(const char* status) {
    OttoEmojiDisplay::SetStatus(status);
    if (!status) {
        return;
    }
    auto kind = NailongSceneState::Status::System;
    if (strcmp(status, Lang::Strings::STANDBY) == 0) {
        kind = NailongSceneState::Status::Standby;
    } else if (strcmp(status, Lang::Strings::SPEAKING) == 0) {
        kind = NailongSceneState::Status::Speaking;
    } else if (strcmp(status, Lang::Strings::LISTENING) == 0 ||
               strcmp(status, Lang::Strings::CONNECTING) == 0) {
        kind = NailongSceneState::Status::Listening;
    }
    DisplayLockGuard lock(this);
    scene_.SetStatus(kind, esp_random());
    if (scene_.ready() && !assets_available_) {
        assets_available_ = CheckImagesLocked();
    }
    if (scene_.ready()) {
        lv_obj_remove_flag(network_label_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(battery_label_, LV_OBJ_FLAG_HIDDEN);
    }
    RefreshImageLocked();
}

void NailongDisplay::SetEmotion(const char* emotion) {
    {
        DisplayLockGuard lock(this);
        if (assets_available_ && scene_.Image(false) >= 0) {
            RefreshImageLocked();
            return;
        }
    }
    OttoEmojiDisplay::SetEmotion(emotion);
}

void NailongDisplay::SetTheme(Theme* theme) {
    OttoEmojiDisplay::SetTheme(theme);
    DisplayLockGuard lock(this);
    current_image_ = -1;
    if (scene_.ready()) {
        assets_available_ = CheckImagesLocked();
        RefreshImageLocked();
    }
}

void NailongDisplay::SetLaughing(bool laughing) {
    DisplayLockGuard lock(this);
    scene_.SetLaughing(laughing);
    RefreshImageLocked();
}

void NailongDisplay::RefreshImageLocked() {
    if (!nailong_image_ || !assets_available_) {
        return;
    }
    int index = scene_.Image(Application::GetInstance().GetAudioService().IsPlaybackIdle());
    if (index < 0) {
        lv_obj_add_flag(nailong_image_, LV_OBJ_FLAG_HIDDEN);
        current_image_ = -1;
        return;
    }
    if (index == current_image_) {
        return;
    }
    void* ptr = nullptr;
    size_t size = 0;
    if (!Assets::GetInstance().GetAssetData(kImages[index], ptr, size) || size != kImageBytes) {
        return;
    }
    if (gif_controller_) {
        gif_controller_->Stop();
        gif_controller_.reset();
    }
    lv_image_cache_drop(&image_desc_);
    image_desc_.header.magic = LV_IMAGE_HEADER_MAGIC;
    image_desc_.header.cf = LV_COLOR_FORMAT_RGB565;
    image_desc_.header.w = 240;
    image_desc_.header.h = 240;
    image_desc_.header.stride = 480;
    image_desc_.data_size = size;
    // Packed assets need not be aligned. Use one reusable aligned PSRAM framebuffer.
    std::memcpy(pixels_, ptr, size);
    image_desc_.data = pixels_;
    lv_image_set_src(nailong_image_, &image_desc_);
    lv_obj_invalidate(nailong_image_);
    lv_obj_add_flag(emoji_label_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(emoji_image_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(nailong_image_, LV_OBJ_FLAG_HIDDEN);
    current_image_ = index;
    ESP_LOGI("NailongDisplay", "Image: %s", kImages[index]);
}
