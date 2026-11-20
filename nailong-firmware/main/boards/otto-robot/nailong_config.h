#pragma once

#include "config.h"

// Retain the stock pinouts; only this named variant disconnects the two arms.
constexpr HardwareConfig NAILONG_VERSION_CONFIG = [] {
    auto config = NON_CAMERA_VERSION_CONFIG;
    config.left_hand_pin = GPIO_NUM_NC;
    config.right_hand_pin = GPIO_NUM_NC;
    return config;
}();

constexpr bool NailongPinsAreUnique() {
    constexpr gpio_num_t pins[] = {
        NAILONG_VERSION_CONFIG.left_leg_pin,
        NAILONG_VERSION_CONFIG.right_leg_pin,
        NAILONG_VERSION_CONFIG.left_foot_pin,
        NAILONG_VERSION_CONFIG.right_foot_pin,
        NAILONG_VERSION_CONFIG.left_hand_pin,
        NAILONG_VERSION_CONFIG.right_hand_pin,
        NAILONG_VERSION_CONFIG.audio_i2s_gpio_ws,
        NAILONG_VERSION_CONFIG.audio_i2s_gpio_bclk,
        NAILONG_VERSION_CONFIG.audio_i2s_gpio_din,
        NAILONG_VERSION_CONFIG.audio_i2s_gpio_dout,
        NAILONG_VERSION_CONFIG.audio_i2s_mic_gpio_ws,
        NAILONG_VERSION_CONFIG.audio_i2s_mic_gpio_sck,
        NAILONG_VERSION_CONFIG.audio_i2s_mic_gpio_din,
        NAILONG_VERSION_CONFIG.audio_i2s_spk_gpio_dout,
        NAILONG_VERSION_CONFIG.audio_i2s_spk_gpio_bclk,
        NAILONG_VERSION_CONFIG.audio_i2s_spk_gpio_lrck,
        NAILONG_VERSION_CONFIG.display_backlight_pin,
        NAILONG_VERSION_CONFIG.display_mosi_pin,
        NAILONG_VERSION_CONFIG.display_clk_pin,
        NAILONG_VERSION_CONFIG.display_dc_pin,
        NAILONG_VERSION_CONFIG.display_rst_pin,
        NAILONG_VERSION_CONFIG.display_cs_pin,
        NAILONG_VERSION_CONFIG.power_charge_detect_pin,
        NAILONG_VERSION_CONFIG.i2c_sda_pin,
        NAILONG_VERSION_CONFIG.i2c_scl_pin,
        BOOT_BUTTON_GPIO,
    };
    for (unsigned i = 0; i < sizeof(pins) / sizeof(pins[0]); ++i) {
        if (pins[i] == GPIO_NUM_NC) {
            continue;
        }
        for (unsigned j = i + 1; j < sizeof(pins) / sizeof(pins[0]); ++j) {
            if (pins[i] == pins[j]) {
                return false;
            }
        }
    }
    return true;
}

static_assert(NAILONG_VERSION_CONFIG.left_hand_pin == GPIO_NUM_NC &&
                  NAILONG_VERSION_CONFIG.right_hand_pin == GPIO_NUM_NC,
              "Nailong must not initialize arm servos");
static_assert(NailongPinsAreUnique(), "Nailong active GPIO assignments must not overlap");
