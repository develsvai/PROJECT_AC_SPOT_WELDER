#include "../project/ACSpotWelderNewArch/app.h"

#include <stdbool.h>
#include <stdio.h>

#include "../project/ACSpotWelderNewArch/hardware.h"
#include "../project/ACSpotWelderNewArch/types.h"
#include "hardware_sim.h"
#include "settings_sim.h"

static void sim_run_for_ms(uint32_t duration_ms)
{
    uint32_t steps = duration_ms * 10U;

    while (steps-- > 0U) {
        sim_hardware_step_us(100U);
        app_run();
    }
}

static void sim_press_manual_for_ms(uint32_t duration_ms)
{
    sim_hardware_set_manual_pressed(true);
    sim_run_for_ms(duration_ms);
    sim_hardware_set_manual_pressed(false);
}

static void sim_log_outputs(const char *label)
{
    printf(
        "[%6lu ms] %-12s triac=%d buzzer=%d sense=%d\n",
        (unsigned long)hardware_millis(),
        label,
        sim_hardware_triac_is_on() ? 1 : 0,
        sim_hardware_buzzer_is_on() ? 1 : 0,
        sim_hardware_sense_is_on() ? 1 : 0);
}

static void run_manual_scenario(void)
{
    const AppContext *app;

    printf("\n=== Manual Weld Scenario ===\n");
    settings_sim_set_defaults(50U, 2U, 100U, MODE_MANUAL);
    app_init();

    sim_run_for_ms(1600U);
    sim_log_outputs("pre-manual");

    sim_press_manual_for_ms(20U);
    sim_run_for_ms(500U);

    app = app_get_context();
    printf(
        "[%6lu ms] manual result state=%d pulses_remaining=%u\n",
        (unsigned long)hardware_millis(),
        app->process.state,
        app->process.pulses_remaining);
    sim_log_outputs("post-manual");
}

static void run_auto_scenario(void)
{
    const AppContext *app;

    printf("\n=== Auto Detect Scenario ===\n");
    settings_sim_set_defaults(50U, 1U, 100U, MODE_AUTO);
    app_init();

    sim_run_for_ms(1600U);
    sim_log_outputs("baseline");

    sim_hardware_set_contact(true);
    sim_run_for_ms(300U);
    sim_hardware_set_contact(false);

    sim_run_for_ms(600U);

    app = app_get_context();
    printf(
        "[%6lu ms] auto result state=%d auto_state=%d trigger=%d\n",
        (unsigned long)hardware_millis(),
        app->process.state,
        app->auto_detect.state,
        app->process.auto_trigger_request ? 1 : 0);
    sim_log_outputs("post-auto");
}

int main(void)
{
    run_manual_scenario();
    run_auto_scenario();
    return 0;
}
