#include "settings_sim.h"

#include "../project/ACSpotWelderNewArch/settings.h"

static Settings g_defaults = {50U, 1U, 200U, MODE_MANUAL};
static Settings g_saved = {50U, 1U, 200U, MODE_MANUAL};

void settings_sim_set_defaults(uint16_t time_ms, uint8_t multiplier, uint16_t rest_ms, WeldMode mode)
{
    g_defaults.time_ms = time_ms;
    g_defaults.multiplier = multiplier;
    g_defaults.rest_ms = rest_ms;
    g_defaults.mode = mode;
    g_saved = g_defaults;
}

const Settings *settings_sim_get_saved(void)
{
    return &g_saved;
}

void settings_load(Settings *settings)
{
    *settings = g_defaults;
}

void settings_save(const Settings *settings)
{
    g_saved = *settings;
}
