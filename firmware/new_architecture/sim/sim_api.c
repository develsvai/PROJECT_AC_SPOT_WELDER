#include "sim_api.h"

#include <string.h>

#include "../project/ACSpotWelderNewArch/app.h"
#include "../project/ACSpotWelderNewArch/app_config.h"
#include "../project/ACSpotWelderNewArch/hardware.h"
#include "../project/ACSpotWelderNewArch/types.h"
#include "hardware_sim.h"
#include "render_sim.h"
#include "settings_sim.h"

static uint32_t g_weld_count = 0U;
static ProcessState g_last_process_state = PROC_READY;

static void sim_api_run_for_ms(uint32_t duration_ms)
{
    uint32_t steps = duration_ms * 10U;

    while (steps-- > 0U) {
        sim_hardware_step_us(100U);
        app_run();
        {
            const AppContext *app = app_get_context();
            if (g_last_process_state != PROC_DONE && app->process.state == PROC_DONE) {
                g_weld_count++;
            }
            g_last_process_state = app->process.state;
        }
    }
}

void sim_api_init(void)
{
    settings_sim_set_defaults(50U, 1U, 200U, MODE_MANUAL);
    app_init();
    g_weld_count = 0U;
    g_last_process_state = app_get_context()->process.state;
}

void sim_api_step_ms(uint32_t duration_ms)
{
    sim_api_run_for_ms(duration_ms);
}

void sim_api_press_manual(void)
{
    sim_hardware_set_manual_pressed(true);
    sim_api_run_for_ms(20U);
    sim_hardware_set_manual_pressed(false);
    sim_api_run_for_ms(1U);
}

void sim_api_manual_cycle(void)
{
    const AppContext *app = app_get_context();
    uint32_t settle_ms =
        ((uint32_t)app->settings.time_ms * app->settings.multiplier) +
        ((uint32_t)app->settings.rest_ms * (app->settings.multiplier > 0U ?
            (uint32_t)(app->settings.multiplier - 1U) : 0U)) +
        LOCKOUT_TIME_MS + 100U;

    sim_api_press_manual();
    sim_api_run_for_ms(settle_ms);
}

void sim_api_press_encoder(void)
{
    sim_hardware_set_encoder_pressed(true);
    sim_api_run_for_ms(20U);
    sim_hardware_set_encoder_pressed(false);
    sim_api_run_for_ms(1U);
}

void sim_api_rotate_encoder(int8_t delta)
{
    app_inject_encoder_delta(delta);
    sim_api_run_for_ms(1U);
}

void sim_api_set_contact(bool active)
{
    sim_hardware_set_contact(active);
}

void sim_api_toggle_contact(void)
{
    sim_hardware_set_contact(!sim_hardware_contact_is_on());
}

void sim_api_touch_pulse(uint32_t contact_ms, uint32_t settle_ms)
{
    sim_hardware_set_contact(true);
    sim_api_run_for_ms(contact_ms);
    sim_hardware_set_contact(false);
    sim_api_run_for_ms(settle_ms);
}

void sim_api_set_settings(uint16_t time_ms, uint8_t multiplier, uint16_t rest_ms, uint8_t mode)
{
    Settings next = {
        .time_ms = time_ms,
        .multiplier = multiplier,
        .rest_ms = rest_ms,
        .mode = (WeldMode)mode,
    };

    settings_sim_set_defaults(next.time_ms, next.multiplier, next.rest_ms, next.mode);
    app_apply_settings(&next);
    sim_api_run_for_ms(1U);
}

void sim_api_get_snapshot(SimSnapshot *snapshot)
{
    const AppContext *app = app_get_context();

    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->time_ms = hardware_millis();
    snapshot->setting_time_ms = app->settings.time_ms;
    snapshot->setting_multiplier = app->settings.multiplier;
    snapshot->setting_rest_ms = app->settings.rest_ms;
    snapshot->setting_mode = (uint8_t)app->settings.mode;
    snapshot->ui_state = (uint8_t)app->ui.state;
    snapshot->menu_item = (uint8_t)app->ui.selected_item;
    snapshot->edit_value = app->ui.edit_value;
    snapshot->process_state = (uint8_t)app->process.state;
    snapshot->process_pulses_remaining = app->process.pulses_remaining;
    snapshot->auto_state = (uint8_t)app->auto_detect.state;
    snapshot->auto_baseline_feature = app->auto_detect.baseline_feature;
    snapshot->auto_half_feature = app->auto_detect.half_feature;
    snapshot->triac_on = sim_hardware_triac_is_on();
    snapshot->buzzer_on = sim_hardware_buzzer_is_on();
    snapshot->sense_on = sim_hardware_sense_is_on();
    snapshot->contact_on = sim_hardware_contact_is_on();
    snapshot->pulse_count = sim_hardware_pulse_count();
    snapshot->weld_count = g_weld_count;
    strncpy(snapshot->lcd_line0, render_sim_get_line0(), 16);
    strncpy(snapshot->lcd_line1, render_sim_get_line1(), 16);
    snapshot->lcd_line0[16] = '\0';
    snapshot->lcd_line1[16] = '\0';
}
