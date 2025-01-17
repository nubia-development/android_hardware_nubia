//
// Copyright (C) 2023 The LineageOS Project
//
// SPDX-License-Identifier: Apache-2.0
//

#include "Lights.h"

#include <android-base/logging.h>
#include <fstream>

#define BATTERY_CAPACITY            "/sys/class/power_supply/battery/capacity"
#define BATTERY_STATUS_FILE         "/sys/class/power_supply/battery/status"

#define BATTERY_STATUS_CHARGING     "Charging"
#define BATTERY_STATUS_DISCHARGING  "Discharging"
#define BATTERY_STATUS_FULL         "Full"

enum battery_status {
    BATTERY_UNKNOWN = 0,
    BATTERY_LOW,
    BATTERY_FREE,
    BATTERY_CHARGING,
    BATTERY_FULL,
};

namespace {
/*
 * Write value to path and close file.
 */
static void set(std::string path, std::string value) {
    std::ofstream file(path);

    if (!file.is_open()) {
        LOG(WARNING) << "failed to write " << value.c_str() << " to " << path.c_str();
        return;
    }

    file << value;
}

static int get(std::string path) {
    std::ifstream file(path);
    int value;

    if (!file.is_open()) {
        LOG(WARNING) << "failed to read from: " << path.c_str();
        return 0;
    }

    file >> value;
    return value;
}

static int readStr(std::string path, char *buffer, size_t size)
{

    std::ifstream file(path);

    if (!file.is_open()) {
        LOG(WARNING) << "failed to read: " << path.c_str();
        return -1;
    }

    file.read(buffer, size);
    file.close();
    return 1;
}

static void set(std::string path, int value) {
    set(path, std::to_string(value));
}

int getBatteryStatus()
{
    int err;

    char status_str[16];
    int capacity = 0;

    err = readStr(BATTERY_STATUS_FILE, status_str, sizeof(status_str));
    if (err <= 0) {
        LOG(WARNING) << "failed to read battery status: " << err;
        return BATTERY_UNKNOWN;
    }

    capacity = get(BATTERY_CAPACITY);

    if (0 == strncmp(status_str, BATTERY_STATUS_FULL, 4)) {
            return BATTERY_FULL;
        }

    if (0 == strncmp(status_str, BATTERY_STATUS_DISCHARGING, 11)) {
            return BATTERY_FREE;
        }

    if (0 == strncmp(status_str, BATTERY_STATUS_CHARGING, 8)) {
        if (capacity < 90) {
            return BATTERY_CHARGING;
        } else {
            return BATTERY_FULL;
        }
    } else {
        if (capacity < 10) {
            return BATTERY_LOW;
        } else {
            return BATTERY_FREE;
        }
    }
}

#if defined(LCD_NODE)
static uint32_t getBrightness(const HwLightState& state) {
    uint32_t alpha, red, green, blue;

    /*
     * Extract brightness from AARRGGBB.
     */
    alpha = (state.color >> 24) & 0xFF;
    red = (state.color >> 16) & 0xFF;
    green = (state.color >> 8) & 0xFF;
    blue = state.color & 0xFF;

    /*
     * Scale RGB brightness if Alpha brightness is not 0xFF.
     */
    if (alpha != 0xFF) {
        red = red * alpha / 0xFF;
        green = green * alpha / 0xFF;
        blue = blue * alpha / 0xFF;
    }

    return (77 * red + 150 * green + 29 * blue) >> 8;
}

static inline uint32_t scaleBrightness(uint32_t brightness, uint32_t maxBrightness) {
    // Map brightness values logarithmatically to match aosp behaviour
    LOG(DEBUG) << "Received brightness: " << brightness;
    if (maxBrightness == MAX_LCD_BRIGHTNESS)
        return brightness_table[brightness];
    return brightness;
}

static inline uint32_t getScaledBrightness(const HwLightState& state, uint32_t maxBrightness) {
    return scaleBrightness(getBrightness(state), maxBrightness);
}

static void handleBacklight(const HwLightState& state) {
    uint32_t brightness = getScaledBrightness(state, MAX_LCD_BRIGHTNESS);
    set(LCD_NODE, brightness);
}
#endif

static void handleNotification(const HwLightState& state) {

#if defined(RGB_LED_RED) && defined(RGB_LED_GREEN)
    uint32_t brightness = (state.color >> 24) & 0xFF;
    uint32_t red = ((state.color >> 16) & 0xFF) * brightness / 0xFF;
    uint32_t green = ((state.color >> 8) & 0xFF) * brightness / 0xFF;
    uint32_t blue = (state.color & 0xFF) * brightness / 0xFF;
#endif

    switch (state.flashMode) {
        case FlashMode::HARDWARE:
        case FlashMode::TIMED:
#ifdef AW22XX_LED
            set(LED_NODE AW22XX_LED LED_EFFECT, LED_NOTIFICATION);
#endif

#ifdef LED_BREATH_FEATURE
            set(LED_NODE RGB_LED_BLUE LED_BREATH_FEATURE, LOGO_LED_BREATH);
#endif

#if defined(RGB_LED_RED) && defined(RGB_LED_GREEN)
            /* Enable blinking */
            if (!!red)
                set(LED_NODE RGB_LED_RED LED_DELAY_ON, state.flashOnMs);
                set(LED_NODE RGB_LED_RED LED_DELAY_OFF, state.flashOffMs);

            if (!!green)
                set(LED_NODE RGB_LED_GREEN LED_DELAY_ON, state.flashOnMs);
                set(LED_NODE RGB_LED_GREEN LED_DELAY_OFF, state.flashOffMs);

            if (!!blue)
                set(LED_NODE RGB_LED_BLUE LED_DELAY_ON, state.flashOnMs);
                set(LED_NODE RGB_LED_BLUE LED_DELAY_OFF, state.flashOffMs);
#endif

#ifdef NUBIA_LED
            set(LED_NODE NUBIA_LED LED_COLOR, COLOR_GREEN);
            set(LED_NODE NUBIA_LED LED_FADE, "3 0 4");
            set(LED_NODE NUBIA_LED LED_GRADE, "0 100");
            set(LED_NODE NUBIA_LED LED_BLINK_MODE, BLINK_ON);
#endif
            break;
        case FlashMode::NONE:
        default:
	    int battery_state = getBatteryStatus();
	    if (battery_state == BATTERY_CHARGING || battery_state == BATTERY_LOW) {
#if defined(RGB_LED_RED) && defined(RGB_LED_GREEN)
                set(LED_NODE RGB_LED_GREEN LED_BRIGHTNESS, 0);
                set(LED_NODE RGB_LED_RED LED_BRIGHTNESS, red);
                set(LED_NODE RGB_LED_BLUE LED_BRIGHTNESS, blue);
#endif

#ifdef NUBIA_LED
                set(LED_NODE NUBIA_LED LED_COLOR, COLOR_RED);
                set(LED_NODE NUBIA_LED LED_FADE, "0 0 0");
                set(LED_NODE NUBIA_LED LED_GRADE, "100 255");
                set(LED_NODE NUBIA_LED LED_BLINK_MODE, BLINK_CONST);
#endif

#ifdef AW22XX_LED
                set(LED_NODE AW22XX_LED LED_EFFECT, LED_BATTERY_CHARGING);
#endif

#ifdef LED_BREATH_FEATURE
                set(LED_NODE RGB_LED_BLUE LED_BREATH_FEATURE, LOGO_LED_ON);
#endif
            } else if (battery_state == BATTERY_FULL) {
#if defined(RGB_LED_RED) && defined(RGB_LED_GREEN)
                set(LED_NODE RGB_LED_RED LED_BRIGHTNESS, 0);
                set(LED_NODE RGB_LED_GREEN LED_BRIGHTNESS, green);
                set(LED_NODE RGB_LED_BLUE LED_BRIGHTNESS, blue);
#endif

#ifdef NUBIA_LED
                set(LED_NODE NUBIA_LED LED_COLOR, COLOR_GREEN);
                set(LED_NODE NUBIA_LED LED_FADE, "0 0 0");
                set(LED_NODE NUBIA_LED LED_GRADE, "100 255");
                set(LED_NODE NUBIA_LED LED_BLINK_MODE, BLINK_CONST);
#endif

#ifdef AW22XX_LED
                set(LED_NODE AW22XX_LED LED_EFFECT, LED_BATTERY_FULL);
#endif

#ifdef LED_BREATH_FEATURE
                set(LED_NODE RGB_LED_BLUE LED_BREATH_FEATURE, LOGO_LED_ON);
#endif
            } else if (battery_state == BATTERY_FREE) {
#if defined(RGB_LED_RED) && defined(RGB_LED_GREEN)
                set(LED_NODE RGB_LED_RED LED_BRIGHTNESS, 0);
                set(LED_NODE RGB_LED_GREEN LED_BRIGHTNESS, 0);
                set(LED_NODE RGB_LED_BLUE LED_BRIGHTNESS, 0);
#endif

#ifdef NUBIA_LED
                set(LED_NODE NUBIA_LED LED_BRIGHTNESS, 0);
#endif

#ifdef AW22XX_LED
                set(LED_NODE AW22XX_LED LED_EFFECT, LED_OFF);
#endif

#ifdef LED_BREATH_FEATURE
                set(LED_NODE RGB_LED_BLUE LED_BREATH_FEATURE, LOGO_LED_OFF);
#endif
            }
            break;
    }
    return;
}

/* Keep sorted in the order of importance. */
static std::vector<LightType> backends = {
    LightType::ATTENTION,
#ifdef LCD_NODE
    LightType::BACKLIGHT,
#endif
    LightType::BATTERY,
    LightType::NOTIFICATIONS,
};

}  // anonymous namespace

namespace aidl {
namespace android {
namespace hardware {
namespace light {

ndk::ScopedAStatus Lights::setLightState(int id, const HwLightState& state) {
    LOG(INFO) << "Lights setting state for id=" << id << " to color " << std::hex << state.color;
    switch(id) {
        case (int) LightType::ATTENTION:
            handleNotification(state);
            return ndk::ScopedAStatus::ok();
#ifdef LCD_NODE
        case (int) LightType::BACKLIGHT:
            handleBacklight(state);
            return ndk::ScopedAStatus::ok();
#endif
        case (int) LightType::BATTERY:
            handleNotification(state);
            return ndk::ScopedAStatus::ok();
        case (int) LightType::NOTIFICATIONS:
            handleNotification(state);
            return ndk::ScopedAStatus::ok();
        default:
            return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }
}

ndk::ScopedAStatus Lights::getLights(std::vector<HwLight>* lights) {
    LOG(INFO) << "Lights reporting supported lights";
    int i = 0;

    for (const LightType& backend : backends) {
        HwLight hwLight;
        hwLight.id = (int) backend;
        hwLight.type = backend;
        hwLight.ordinal = i;
        lights->push_back(hwLight);
        i++;
    }

    return ndk::ScopedAStatus::ok();
}

}  // namespace light
}  // namespace hardware
}  // namespace android
}  // namespace aidl
