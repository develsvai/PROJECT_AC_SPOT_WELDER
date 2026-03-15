#include "auto_detect.h"

#include "app_config.h"
#include "hardware.h"

static bool auto_detect_is_contact(uint16_t feature, uint16_t baseline)
{
    uint16_t ratio_threshold =
        (uint16_t)(((uint32_t)baseline * FEATURE_RATIO_NUM) / FEATURE_RATIO_DEN);

    return (feature > ratio_threshold) && (feature > (uint16_t)(baseline + FEATURE_MIN_DIFF));
}

static void auto_detect_update_baseline(AutoDetectContext *auto_detect, uint16_t feature)
{
    int16_t delta = (int16_t)feature - (int16_t)auto_detect->baseline_feature;
    auto_detect->baseline_feature =
        (uint16_t)(auto_detect->baseline_feature + (delta >> BASELINE_ALPHA_SHIFT));

    if (auto_detect->baseline_feature == 0U) {
        auto_detect->baseline_feature = 1U;
    }
}

void auto_detect_init(AutoDetectContext *auto_detect)
{
    auto_detect->state = AUTO_IDLE;
    auto_detect->adc_center = 128U;
    auto_detect->baseline_feature = 10U;
    auto_detect->abs_sum = 0U;
    auto_detect->sample_count = 0U;
    auto_detect->half_feature = 0U;
    auto_detect->detect_count = 0U;
    auto_detect->release_count = 0U;
    auto_detect->half_active = false;
    auto_detect->feature_ready = false;
    auto_detect->trigger_request = false;
}

void auto_detect_reset(AutoDetectContext *auto_detect)
{
    auto_detect->state = AUTO_IDLE;
    auto_detect->abs_sum = 0U;
    auto_detect->sample_count = 0U;
    auto_detect->half_feature = 0U;
    auto_detect->detect_count = 0U;
    auto_detect->release_count = 0U;
    auto_detect->half_active = false;
    auto_detect->feature_ready = false;
    auto_detect->trigger_request = false;
}

void auto_detect_handle_zero_cross(AutoDetectContext *auto_detect)
{
    if (auto_detect->half_active && auto_detect->sample_count > 0U) {
        auto_detect->half_feature =
            (uint16_t)(auto_detect->abs_sum / auto_detect->sample_count);
        auto_detect->feature_ready = true;
    }

    auto_detect->abs_sum = 0U;
    auto_detect->sample_count = 0U;
    auto_detect->half_active = true;

    if (!auto_detect->feature_ready) {
        return;
    }

    auto_detect->feature_ready = false;

    switch (auto_detect->state) {
        case AUTO_IDLE:
            if (auto_detect_is_contact(auto_detect->half_feature, auto_detect->baseline_feature)) {
                auto_detect->detect_count = 1U;
                auto_detect->release_count = 0U;
                auto_detect->state = AUTO_CANDIDATE;
            } else {
                auto_detect_update_baseline(auto_detect, auto_detect->half_feature);
            }
            break;

        case AUTO_CANDIDATE:
            if (auto_detect_is_contact(auto_detect->half_feature, auto_detect->baseline_feature)) {
                auto_detect->detect_count++;
                if (auto_detect->detect_count >= DETECT_CONSECUTIVE_COUNT) {
                    auto_detect->trigger_request = true;
                    auto_detect->state = AUTO_LOCKOUT;
                }
            } else {
                auto_detect->release_count++;
                if (auto_detect->release_count >= RELEASE_CONSECUTIVE_COUNT) {
                    auto_detect->detect_count = 0U;
                    auto_detect->release_count = 0U;
                    auto_detect->state = AUTO_IDLE;
                    auto_detect_update_baseline(auto_detect, auto_detect->half_feature);
                }
            }
            break;

        case AUTO_LOCKOUT:
            if (auto_detect_is_contact(auto_detect->half_feature, auto_detect->baseline_feature)) {
                auto_detect->release_count = 0U;
            } else {
                auto_detect->release_count++;
                if (auto_detect->release_count >= RELEASE_CONSECUTIVE_COUNT) {
                    auto_detect->detect_count = 0U;
                    auto_detect->release_count = 0U;
                    auto_detect->state = AUTO_IDLE;
                    auto_detect_update_baseline(auto_detect, auto_detect->half_feature);
                }
            }
            break;
    }
}

void auto_detect_handle_sample_tick(AutoDetectContext *auto_detect)
{
    uint8_t raw;
    int16_t diff;
    int16_t center_delta;

    if (!auto_detect->half_active || auto_detect->sample_count >= MAX_SAMPLES_PER_HALF) {
        return;
    }

    raw = hardware_adc_read_8bit();
    center_delta = (int16_t)raw - (int16_t)auto_detect->adc_center;
    auto_detect->adc_center = (uint16_t)(auto_detect->adc_center + (center_delta >> CENTER_ALPHA_SHIFT));

    diff = (int16_t)raw - (int16_t)auto_detect->adc_center;
    if (diff < 0) {
        diff = -diff;
    }

    auto_detect->abs_sum += (uint16_t)diff;
    auto_detect->sample_count++;
}

bool auto_detect_consume_trigger(AutoDetectContext *auto_detect)
{
    bool state = auto_detect->trigger_request;
    auto_detect->trigger_request = false;
    return state;
}
