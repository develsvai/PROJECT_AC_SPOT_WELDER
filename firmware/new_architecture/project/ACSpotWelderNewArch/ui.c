#include "ui.h"

#include "app_config.h"

static int16_t ui_clamp_edit_value(MenuItem item, int16_t value)
{
    switch (item) {
        case MENU_TIME:
            if (value < (int16_t)TIME_MIN_MS) {
                value = (int16_t)TIME_MIN_MS;
            }
            if (value > (int16_t)TIME_MAX_MS) {
                value = (int16_t)TIME_MAX_MS;
            }
            break;
        case MENU_MULT:
            if (value < (int16_t)MULT_MIN) {
                value = (int16_t)MULT_MIN;
            }
            if (value > (int16_t)MULT_MAX) {
                value = (int16_t)MULT_MAX;
            }
            break;
        case MENU_REST:
            if (value < (int16_t)REST_MIN_MS) {
                value = (int16_t)REST_MIN_MS;
            }
            if (value > (int16_t)REST_MAX_MS) {
                value = (int16_t)REST_MAX_MS;
            }
            break;
        case MENU_MODE:
            if (value < (int16_t)MODE_MANUAL) {
                value = (int16_t)MODE_MANUAL;
            }
            if (value > (int16_t)MODE_AUTO) {
                value = (int16_t)MODE_AUTO;
            }
            break;
        default:
            break;
    }
    return value;
}

void ui_init(UiContext *ui)
{
    ui->state = UI_SPLASH;
    ui->selected_item = MENU_TIME;
    ui->edit_value = 0;
    ui->timeout_ms = UI_TIMEOUT_MS;
    ui->splash_ms = SPLASH_TIME_MS;
    ui->dirty = true;
}

bool ui_tick(UiContext *ui, Settings *settings, const InputEvent *input, bool process_busy)
{
    bool save_requested = false;
    (void)process_busy;

    if (ui->state != UI_SPLASH && ui->timeout_ms > 0U) {
        ui->timeout_ms--;
    }

    switch (ui->state) {
        case UI_SPLASH:
            if (ui->splash_ms > 0U) {
                ui->splash_ms--;
            } else {
                ui->state = UI_IDLE;
                ui->dirty = true;
            }
            break;

        case UI_IDLE:
            if (input->enc_pressed) {
                ui->state = UI_MENU_MAIN;
                ui->timeout_ms = UI_TIMEOUT_MS;
                ui->dirty = true;
            }
            break;

        case UI_MENU_MAIN:
            if (ui->timeout_ms == 0U || input->manual_pressed) {
                ui->state = UI_IDLE;
                ui->dirty = true;
                break;
            }

            if (input->encoder_delta > 0) {
                ui->selected_item = (MenuItem)((ui->selected_item + 1U) % MENU_COUNT);
                ui->timeout_ms = UI_TIMEOUT_MS;
                ui->dirty = true;
            } else if (input->encoder_delta < 0) {
                ui->selected_item = (MenuItem)((ui->selected_item + MENU_COUNT - 1U) % MENU_COUNT);
                ui->timeout_ms = UI_TIMEOUT_MS;
                ui->dirty = true;
            }

            if (input->enc_pressed) {
                switch (ui->selected_item) {
                    case MENU_TIME:
                        ui->edit_value = (int16_t)settings->time_ms;
                        break;
                    case MENU_MULT:
                        ui->edit_value = (int16_t)settings->multiplier;
                        break;
                    case MENU_REST:
                        ui->edit_value = (int16_t)settings->rest_ms;
                        break;
                    case MENU_MODE:
                        ui->edit_value = (int16_t)settings->mode;
                        break;
                    default:
                        break;
                }
                ui->state = UI_MENU_EDIT;
                ui->timeout_ms = UI_TIMEOUT_MS;
                ui->dirty = true;
            }
            break;

        case UI_MENU_EDIT:
            if (ui->timeout_ms == 0U || input->manual_pressed) {
                ui->state = UI_IDLE;
                ui->dirty = true;
                break;
            }

            if (input->encoder_delta != 0) {
                ui->edit_value = ui_clamp_edit_value(
                    ui->selected_item, (int16_t)(ui->edit_value + input->encoder_delta));
                ui->timeout_ms = UI_TIMEOUT_MS;
                ui->dirty = true;
            }

            if (input->enc_pressed) {
                switch (ui->selected_item) {
                    case MENU_TIME:
                        settings->time_ms = (uint16_t)ui->edit_value;
                        break;
                    case MENU_MULT:
                        settings->multiplier = (uint8_t)ui->edit_value;
                        break;
                    case MENU_REST:
                        settings->rest_ms = (uint16_t)ui->edit_value;
                        break;
                    case MENU_MODE:
                        settings->mode = (WeldMode)ui->edit_value;
                        break;
                    default:
                        break;
                }
                ui->state = UI_IDLE;
                ui->dirty = true;
                save_requested = true;
            }
            break;
    }

    return save_requested;
}
