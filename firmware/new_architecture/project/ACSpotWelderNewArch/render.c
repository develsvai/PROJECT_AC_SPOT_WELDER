#include "render.h"

#include <stdio.h>
#include <string.h>

#include "lcd_i2c.h"

static char g_last_line0[17];
static char g_last_line1[17];

static void render_write_if_changed(uint8_t pos, char *last, const char *next)
{
    if (strncmp(last, next, 16) != 0) {
        LCD_string(pos, next);
        strncpy(last, next, 16);
        last[16] = '\0';
    }
}

static const char *render_menu_label(MenuItem item)
{
    switch (item) {
        case MENU_TIME:
            return "TIME";
        case MENU_MULT:
            return "MULT";
        case MENU_REST:
            return "REST";
        case MENU_MODE:
            return "MODE";
        default:
            return "";
    }
}

void render_init(void)
{
    g_last_line0[0] = '\0';
    g_last_line1[0] = '\0';
    LCD_initialize();
}

void render_tick(const AppContext *app)
{
    char line0[17];
    char line1[17];

    memset(line0, ' ', 16);
    memset(line1, ' ', 16);
    line0[16] = '\0';
    line1[16] = '\0';

    switch (app->ui.state) {
        case UI_SPLASH:
            snprintf(line0, sizeof(line0), " AC SPOT WELDER");
            snprintf(line1, sizeof(line1), " FSM NEW ARCH   ");
            break;

        case UI_MENU_MAIN:
            snprintf(line0, sizeof(line0), "> %-13s", render_menu_label(app->ui.selected_item));
            snprintf(line1, sizeof(line1), "PRESS TO EDIT  ");
            break;

        case UI_MENU_EDIT:
            if (app->ui.selected_item == MENU_MODE) {
                snprintf(line0, sizeof(line0), "SET MODE       ");
                snprintf(line1, sizeof(line1), "< %-11s >", app->ui.edit_value ? "AUTO" : "MANUAL");
            } else {
                snprintf(line0, sizeof(line0), "SET %-12s", render_menu_label(app->ui.selected_item));
                snprintf(line1, sizeof(line1), "< %03d         >", app->ui.edit_value);
            }
            break;

        case UI_IDLE:
        default:
            snprintf(
                line0,
                sizeof(line0),
                "T:%03u M:%02u R:%03u",
                app->settings.time_ms,
                app->settings.multiplier,
                app->settings.rest_ms);

            switch (app->process.state) {
                case PROC_AUTO_MONITOR:
                    snprintf(line1, sizeof(line1), "AUTO READY     ");
                    break;
                case PROC_WAIT_ZC:
                    snprintf(line1, sizeof(line1), "WAIT ZC        ");
                    break;
                case PROC_PULSE_ON:
                    snprintf(line1, sizeof(line1), "WELDING        ");
                    break;
                case PROC_REST:
                    snprintf(line1, sizeof(line1), "REST           ");
                    break;
                case PROC_DONE:
                    snprintf(line1, sizeof(line1), "DONE           ");
                    break;
                case PROC_LOCKOUT:
                    snprintf(line1, sizeof(line1), "LOCKOUT        ");
                    break;
                case PROC_READY:
                default:
                    snprintf(
                        line1,
                        sizeof(line1),
                        "%s READY",
                        app->settings.mode == MODE_AUTO ? "AUTO" : "MANUAL");
                    break;
            }
            break;
    }

    render_write_if_changed(0x80, g_last_line0, line0);
    render_write_if_changed(0xC0, g_last_line1, line1);
}
