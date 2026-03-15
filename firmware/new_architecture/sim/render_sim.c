#include "../project/ACSpotWelderNewArch/render.h"

#include <stdio.h>
#include <string.h>

#include "../project/ACSpotWelderNewArch/hardware.h"
#include "render_sim.h"

static char g_last_line0[17];
static char g_last_line1[17];

static const char *process_name(ProcessState state)
{
    switch (state) {
        case PROC_READY:
            return "READY";
        case PROC_AUTO_MONITOR:
            return "AUTO_MON";
        case PROC_WAIT_ZC:
            return "WAIT_ZC";
        case PROC_PULSE_ON:
            return "PULSE";
        case PROC_REST:
            return "REST";
        case PROC_DONE:
            return "DONE";
        case PROC_LOCKOUT:
            return "LOCKOUT";
        default:
            return "?";
    }
}

void render_init(void)
{
    g_last_line0[0] = '\0';
    g_last_line1[0] = '\0';
}

void render_tick(const AppContext *app)
{
    char line0[17];
    char line1[17];

    snprintf(
        line0,
        sizeof(line0),
        "T:%03u M:%02u R:%03u",
        app->settings.time_ms,
        app->settings.multiplier,
        app->settings.rest_ms);

    snprintf(
        line1,
        sizeof(line1),
        "%s %s",
        app->settings.mode == MODE_AUTO ? "AUTO" : "MAN",
        process_name(app->process.state));

    if (strcmp(line0, g_last_line0) != 0 || strcmp(line1, g_last_line1) != 0) {
        printf("[%6lu ms] LCD | %-16s | %-16s\n", (unsigned long)hardware_millis(), line0, line1);
        strncpy(g_last_line0, line0, sizeof(g_last_line0) - 1);
        strncpy(g_last_line1, line1, sizeof(g_last_line1) - 1);
        g_last_line0[16] = '\0';
        g_last_line1[16] = '\0';
    }
}

const char *render_sim_get_line0(void)
{
    return g_last_line0;
}

const char *render_sim_get_line1(void)
{
    return g_last_line1;
}
