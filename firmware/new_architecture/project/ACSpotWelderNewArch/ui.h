#ifndef UI_H
#define UI_H

#include "types.h"

void ui_init(UiContext *ui);
bool ui_tick(UiContext *ui, Settings *settings, const InputEvent *input, bool process_busy);

#endif
