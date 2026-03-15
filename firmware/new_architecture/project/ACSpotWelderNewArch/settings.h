#ifndef SETTINGS_H
#define SETTINGS_H

#include "types.h"

void settings_load(Settings *settings);
void settings_save(const Settings *settings);

#endif
