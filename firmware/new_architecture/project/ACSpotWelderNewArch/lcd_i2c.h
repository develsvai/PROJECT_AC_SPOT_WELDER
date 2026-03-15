#ifndef LCD_I2C_H
#define LCD_I2C_H

#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

#define LCD_ADDRESS 0x27

static inline void lcd_i2c_transmit(uint8_t addr, uint8_t data)
{
    TWCR = 0xa4;
    while (!(TWCR & 0x80) || ((TWSR & 0xf8) != 0x08)) {
    }
    TWDR = addr << 1;
    TWCR = 0x84;
    while (!(TWCR & 0x80) || ((TWSR & 0xf8) != 0x18)) {
    }
    TWDR = data;
    TWCR = 0x84;
    while (!(TWCR & 0x80) || ((TWSR & 0xf8) != 0x28)) {
    }
    TWCR = 0x94;
}

static inline void lcd_send_nibble(uint8_t value, uint8_t rs_mask)
{
    _delay_us(100);
    lcd_i2c_transmit(LCD_ADDRESS, value | rs_mask);
    lcd_i2c_transmit(LCD_ADDRESS, value | rs_mask | 0x04);
    _delay_us(1);
    lcd_i2c_transmit(LCD_ADDRESS, value | rs_mask);
}

static inline void lcd_command(uint8_t cmd)
{
    lcd_send_nibble(cmd & 0xF0, 0x08);
    lcd_send_nibble((cmd << 4) & 0xF0, 0x08);
}

static inline void lcd_data(uint8_t data)
{
    lcd_send_nibble(data & 0xF0, 0x09);
    lcd_send_nibble((data << 4) & 0xF0, 0x09);
}

static inline void LCD_initialize(void)
{
    TWBR = 10;
    TWSR = 0x00;
    _delay_ms(40);
    lcd_command(0x28);
    _delay_us(50);
    lcd_command(0x0c);
    _delay_us(50);
    lcd_command(0x01);
    _delay_ms(2);
    lcd_command(0x06);
    _delay_us(50);
}

static inline void LCD_string(uint8_t address, const char *str)
{
    uint8_t i = 0;
    lcd_command(address);
    while (*str != '\0') {
        if ((uint8_t)(address + i) == 0x90) {
            lcd_command(0xc0);
        }
        lcd_data((uint8_t)*str++);
        i++;
    }
}

#endif
