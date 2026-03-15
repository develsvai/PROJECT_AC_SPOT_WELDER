#include "settings.h"

#include <avr/eeprom.h>

#include "app_config.h"

#define EEPROM_TIME ((uint16_t *)0)
#define EEPROM_MULT ((uint8_t *)2)
#define EEPROM_REST ((uint16_t *)3)
#define EEPROM_MODE ((uint8_t *)5)

void settings_load(Settings *settings)
{
    settings->time_ms = eeprom_read_word(EEPROM_TIME);
    settings->multiplier = eeprom_read_byte(EEPROM_MULT);
    settings->rest_ms = eeprom_read_word(EEPROM_REST);
    settings->mode = (WeldMode)eeprom_read_byte(EEPROM_MODE);

    if (settings->time_ms < TIME_MIN_MS || settings->time_ms > TIME_MAX_MS) {
        settings->time_ms = 50U;
    }
    if (settings->multiplier < MULT_MIN || settings->multiplier > MULT_MAX) {
        settings->multiplier = 1U;
    }
    if (settings->rest_ms > REST_MAX_MS) {
        settings->rest_ms = 200U;
    }
    if (settings->mode > MODE_AUTO) {
        settings->mode = MODE_MANUAL;
    }
}

void settings_save(const Settings *settings)
{
    eeprom_write_word(EEPROM_TIME, settings->time_ms);
    eeprom_write_byte(EEPROM_MULT, settings->multiplier);
    eeprom_write_word(EEPROM_REST, settings->rest_ms);
    eeprom_write_byte(EEPROM_MODE, (uint8_t)settings->mode);
}
