#ifndef SETTINGS_SIM_H
#define SETTINGS_SIM_H

#include "../project/ACSpotWelderNewArch/types.h"

void settings_sim_set_defaults(uint16_t time_ms, uint8_t multiplier, uint16_t rest_ms, WeldMode mode);
const Settings *settings_sim_get_saved(void);

#endif
