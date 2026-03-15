#include "app.h"

#include <stdbool.h>
#include <string.h>

#include "auto_detect.h"
#include "hardware.h"
#include "input.h"
#include "process.h"
#include "render.h"
#include "settings.h"
#include "types.h"
#include "ui.h"

static AppContext g_app;

static void app_fast_path(void)
{
    if (hardware_consume_sample_tick() && g_app.process.state == PROC_AUTO_MONITOR) {
        auto_detect_handle_sample_tick(&g_app.auto_detect);
    }

    if (hardware_consume_zero_cross()) {
        if (g_app.process.state == PROC_AUTO_MONITOR) {
            auto_detect_handle_zero_cross(&g_app.auto_detect);
            if (auto_detect_consume_trigger(&g_app.auto_detect)) {
                process_request_auto(&g_app.process);
            }
        }

        process_handle_zero_cross(&g_app.process, &g_app.settings);
    }
}

void app_init(void)
{
    memset(&g_app, 0, sizeof(g_app));

    hardware_init();
    settings_load(&g_app.settings);
    input_init(&g_app.input);
    ui_init(&g_app.ui);
    process_init(&g_app.process);
    auto_detect_init(&g_app.auto_detect);
    render_init();
    render_tick(&g_app);
}

void app_run(void)
{
    app_fast_path();

    if (!hardware_consume_tick_1ms()) {
        return;
    }

    input_update(&g_app.input);

    if (g_app.input.manual_pressed &&
        g_app.ui.state == UI_IDLE &&
        !g_app.process.busy &&
        g_app.settings.mode == MODE_MANUAL) {
        process_request_manual(&g_app.process);
    }

    if (ui_tick(&g_app.ui, &g_app.settings, &g_app.input, g_app.process.busy)) {
        settings_save(&g_app.settings);
    }

    process_tick_1ms(&g_app.process, &g_app.settings);

    if (g_app.settings.mode != MODE_AUTO) {
        auto_detect_reset(&g_app.auto_detect);
    }

    render_tick(&g_app);
}

const AppContext *app_get_context(void)
{
    return &g_app;
}

void app_inject_encoder_delta(int8_t delta)
{
    g_app.input.encoder_delta = delta;
}

void app_inject_encoder_press(void)
{
    g_app.input.enc_pressed = true;
}

void app_inject_manual_press(void)
{
    g_app.input.manual_pressed = true;
}

void app_apply_settings(const Settings *settings)
{
    g_app.settings = *settings;
    settings_save(&g_app.settings);

    if (g_app.ui.state == UI_MENU_EDIT) {
        switch (g_app.ui.selected_item) {
            case MENU_TIME:
                g_app.ui.edit_value = (int16_t)g_app.settings.time_ms;
                break;
            case MENU_MULT:
                g_app.ui.edit_value = (int16_t)g_app.settings.multiplier;
                break;
            case MENU_REST:
                g_app.ui.edit_value = (int16_t)g_app.settings.rest_ms;
                break;
            case MENU_MODE:
                g_app.ui.edit_value = (int16_t)g_app.settings.mode;
                break;
            default:
                break;
        }
    }

    render_tick(&g_app);
}
