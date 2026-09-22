/*
 * SPDX-License-Identifier: MIT
 *
 * Write-only HD44780-compatible character LCD driver for the STC C++ library.
 */
#include "LiquidCrystal_backend.h"

#define LCD_FLAG_CONFIGURED  0x01u
#define LCD_FLAG_INITIALIZED 0x02u
#define LCD_REQUIRED_FLAGS   (LCD_FLAG_CONFIGURED | LCD_FLAG_INITIALIZED)
#define LCD_MAX_COLUMNS      40u
#define LCD_MAX_ROWS         4u
#define LCD_DDRAM_CHARACTERS 80u

static STCLiquidCrystalState *lcd_current;
#define lcd_state (*lcd_current)

STCLiquidCrystalState *LiquidCrystal_selectContext(STCLiquidCrystalState *context)
    STC_LIQUIDCRYSTAL_REENTRANT
{
    STCLiquidCrystalState *previous = lcd_current;
    lcd_current = context;
    return previous;
}

static uint8_t lcd_is_ready(void)
{
    return ((lcd_state.flags & LCD_REQUIRED_FLAGS) == LCD_REQUIRED_FLAGS) ?
        1u : 0u;
}

static uint8_t lcd_pins_conflict(uint8_t left, uint8_t right)
                                 STC_LIQUIDCRYSTAL_REENTRANT
{
    if (left == right) {
        return 1u;
    }
#ifdef digitalPinsSharePhysicalPad
    if (digitalPinsSharePhysicalPad(left, right)) {
        return 1u;
    }
#endif
    return 0u;
}

static uint8_t lcd_pin_available(uint8_t pin, uint8_t data_pin_count)
                                 STC_LIQUIDCRYSTAL_REENTRANT
{
    uint8_t index;

    if (digitalPinIsValid(pin) == 0u) {
        return 0u;
    }
    if ((lcd_state.rs_pin != NOT_A_PIN) &&
        (lcd_pins_conflict(pin, lcd_state.rs_pin) != 0u)) {
        return 0u;
    }
    if ((lcd_state.rw_pin != NOT_A_PIN) &&
        (lcd_pins_conflict(pin, lcd_state.rw_pin) != 0u)) {
        return 0u;
    }
    if ((lcd_state.enable_pin != NOT_A_PIN) &&
        (lcd_pins_conflict(pin, lcd_state.enable_pin) != 0u)) {
        return 0u;
    }
    for (index = 0u; index < data_pin_count; ++index) {
        if (lcd_pins_conflict(pin, lcd_state.data_pins[index]) != 0u) {
            return 0u;
        }
    }
    return 1u;
}

static void lcd_release_pins(void)
{
    uint8_t count;
    uint8_t index;

    if ((lcd_state.flags & LCD_FLAG_INITIALIZED) == 0u) {
        return;
    }
    pinMode(lcd_state.rs_pin, INPUT);
    if (lcd_state.rw_pin != NOT_A_PIN) {
        pinMode(lcd_state.rw_pin, INPUT);
    }
    pinMode(lcd_state.enable_pin, INPUT);
    count = ((lcd_state.display_function & LCD_8BITMODE) != 0u) ? 8u : 4u;
    for (index = 0u; index < count; ++index) {
        pinMode(lcd_state.data_pins[index], INPUT);
    }
}

static void lcd_reset_configuration(void)
{
    uint8_t index;

    lcd_release_pins();
    lcd_state.flags = 0u;
    lcd_state.rs_pin = NOT_A_PIN;
    lcd_state.rw_pin = NOT_A_PIN;
    lcd_state.enable_pin = NOT_A_PIN;
    for (index = 0u; index < 8u; ++index) {
        lcd_state.data_pins[index] = NOT_A_PIN;
    }
    lcd_state.columns = 0u;
    lcd_state.rows = 0u;
    lcd_state.display_function = LCD_4BITMODE;
    lcd_state.display_control = 0u;
    lcd_state.display_mode = 0u;
}

static uint8_t lcd_set_control_pins(uint8_t rs_pin, uint8_t rw_pin,
                                    uint8_t enable_pin)
                                    STC_LIQUIDCRYSTAL_REENTRANT
{
    if (lcd_pin_available(rs_pin, 0u) == 0u) {
        return 0u;
    }
    lcd_state.rs_pin = rs_pin;

    if (rw_pin != NOT_A_PIN) {
        if (lcd_pin_available(rw_pin, 0u) == 0u) {
            return 0u;
        }
        lcd_state.rw_pin = rw_pin;
    }

    if (lcd_pin_available(enable_pin, 0u) == 0u) {
        return 0u;
    }
    lcd_state.enable_pin = enable_pin;
    return 1u;
}

static uint8_t lcd_set_data_pin(uint8_t index, uint8_t pin)
                                STC_LIQUIDCRYSTAL_REENTRANT
{
    if (lcd_pin_available(pin, index) == 0u) {
        return 0u;
    }
    lcd_state.data_pins[index] = pin;
    return 1u;
}

static void lcd_pulse_enable(void)
{
    digitalWrite(lcd_state.enable_pin, LOW);
    delayMicroseconds(1u);
    digitalWrite(lcd_state.enable_pin, HIGH);
    delayMicroseconds(1u);
    digitalWrite(lcd_state.enable_pin, LOW);
    delayMicroseconds(100u);
}

static void lcd_prepare_output_low(uint8_t pin)
{
    /* Preload LOW without briefly exposing a reset-high push-pull latch. */
    pinMode(pin, OUTPUT_OPEN_DRAIN);
    digitalWrite(pin, LOW);
    pinMode(pin, OUTPUT);
}

static void lcd_write4bits(uint8_t value)
{
    uint8_t index;

    for (index = 0u; index < 4u; ++index) {
        digitalWrite(lcd_state.data_pins[index],
                     ((value & (uint8_t)(1u << index)) != 0u) ? HIGH : LOW);
    }
    lcd_pulse_enable();
}

static void lcd_write8bits(uint8_t value)
{
    uint8_t index;

    for (index = 0u; index < 8u; ++index) {
        digitalWrite(lcd_state.data_pins[index],
                     ((value & (uint8_t)(1u << index)) != 0u) ? HIGH : LOW);
    }
    lcd_pulse_enable();
}

static void lcd_send(uint8_t value, uint8_t data_mode)
                     STC_LIQUIDCRYSTAL_REENTRANT
{
    if (lcd_is_ready() == 0u) {
        return;
    }

    digitalWrite(lcd_state.rs_pin, (data_mode != 0u) ? HIGH : LOW);
    if (lcd_state.rw_pin != NOT_A_PIN) {
        digitalWrite(lcd_state.rw_pin, LOW);
    }

    if ((lcd_state.display_function & LCD_8BITMODE) != 0u) {
        lcd_write8bits(value);
    } else {
        lcd_write4bits((uint8_t)(value >> 4));
        lcd_write4bits(value);
    }
}

uint8_t LiquidCrystal_setPins(uint8_t rs_pin, uint8_t enable_pin,
                              uint8_t d4_pin, uint8_t d5_pin,
                              uint8_t d6_pin, uint8_t d7_pin)
                              STC_LIQUIDCRYSTAL_REENTRANT
{
    lcd_reset_configuration();
    if (lcd_set_control_pins(rs_pin, NOT_A_PIN, enable_pin) == 0u) {
        return LIQUIDCRYSTAL_ERROR;
    }
    if ((lcd_set_data_pin(0u, d4_pin) == 0u) ||
        (lcd_set_data_pin(1u, d5_pin) == 0u) ||
        (lcd_set_data_pin(2u, d6_pin) == 0u) ||
        (lcd_set_data_pin(3u, d7_pin) == 0u)) {
        return LIQUIDCRYSTAL_ERROR;
    }
    lcd_state.display_function = LCD_4BITMODE;
    lcd_state.flags = LCD_FLAG_CONFIGURED;
    return LIQUIDCRYSTAL_OK;
}

uint8_t LiquidCrystal_setPinsRW(uint8_t rs_pin, uint8_t rw_pin,
                                uint8_t enable_pin, uint8_t d4_pin,
                                uint8_t d5_pin, uint8_t d6_pin,
                                uint8_t d7_pin)
                                STC_LIQUIDCRYSTAL_REENTRANT
{
    lcd_reset_configuration();
    if (lcd_set_control_pins(rs_pin, rw_pin, enable_pin) == 0u) {
        return LIQUIDCRYSTAL_ERROR;
    }
    if ((lcd_set_data_pin(0u, d4_pin) == 0u) ||
        (lcd_set_data_pin(1u, d5_pin) == 0u) ||
        (lcd_set_data_pin(2u, d6_pin) == 0u) ||
        (lcd_set_data_pin(3u, d7_pin) == 0u)) {
        return LIQUIDCRYSTAL_ERROR;
    }
    lcd_state.display_function = LCD_4BITMODE;
    lcd_state.flags = LCD_FLAG_CONFIGURED;
    return LIQUIDCRYSTAL_OK;
}

uint8_t LiquidCrystal_setPins8(uint8_t rs_pin, uint8_t enable_pin,
                               uint8_t d0_pin, uint8_t d1_pin,
                               uint8_t d2_pin, uint8_t d3_pin,
                               uint8_t d4_pin, uint8_t d5_pin,
                               uint8_t d6_pin, uint8_t d7_pin)
                               STC_LIQUIDCRYSTAL_REENTRANT
{
    lcd_reset_configuration();
    if (lcd_set_control_pins(rs_pin, NOT_A_PIN, enable_pin) == 0u) {
        return LIQUIDCRYSTAL_ERROR;
    }
    if ((lcd_set_data_pin(0u, d0_pin) == 0u) ||
        (lcd_set_data_pin(1u, d1_pin) == 0u) ||
        (lcd_set_data_pin(2u, d2_pin) == 0u) ||
        (lcd_set_data_pin(3u, d3_pin) == 0u) ||
        (lcd_set_data_pin(4u, d4_pin) == 0u) ||
        (lcd_set_data_pin(5u, d5_pin) == 0u) ||
        (lcd_set_data_pin(6u, d6_pin) == 0u) ||
        (lcd_set_data_pin(7u, d7_pin) == 0u)) {
        return LIQUIDCRYSTAL_ERROR;
    }
    lcd_state.display_function = LCD_8BITMODE;
    lcd_state.flags = LCD_FLAG_CONFIGURED;
    return LIQUIDCRYSTAL_OK;
}

uint8_t LiquidCrystal_setPins8RW(uint8_t rs_pin, uint8_t rw_pin,
                                 uint8_t enable_pin, uint8_t d0_pin,
                                 uint8_t d1_pin, uint8_t d2_pin,
                                 uint8_t d3_pin, uint8_t d4_pin,
                                 uint8_t d5_pin, uint8_t d6_pin,
                                 uint8_t d7_pin)
                                 STC_LIQUIDCRYSTAL_REENTRANT
{
    lcd_reset_configuration();
    if (lcd_set_control_pins(rs_pin, rw_pin, enable_pin) == 0u) {
        return LIQUIDCRYSTAL_ERROR;
    }
    if ((lcd_set_data_pin(0u, d0_pin) == 0u) ||
        (lcd_set_data_pin(1u, d1_pin) == 0u) ||
        (lcd_set_data_pin(2u, d2_pin) == 0u) ||
        (lcd_set_data_pin(3u, d3_pin) == 0u) ||
        (lcd_set_data_pin(4u, d4_pin) == 0u) ||
        (lcd_set_data_pin(5u, d5_pin) == 0u) ||
        (lcd_set_data_pin(6u, d6_pin) == 0u) ||
        (lcd_set_data_pin(7u, d7_pin) == 0u)) {
        return LIQUIDCRYSTAL_ERROR;
    }
    lcd_state.display_function = LCD_8BITMODE;
    lcd_state.flags = LCD_FLAG_CONFIGURED;
    return LIQUIDCRYSTAL_OK;
}

uint8_t LiquidCrystal_beginWithCharSize(uint8_t columns, uint8_t rows,
                                        uint8_t char_size)
                                        STC_LIQUIDCRYSTAL_REENTRANT
{
    uint8_t data_pin_count;
    uint8_t index;

    if ((lcd_state.flags & LCD_FLAG_CONFIGURED) == 0u) {
        return LIQUIDCRYSTAL_ERROR;
    }
    if ((columns == 0u) || (columns > LCD_MAX_COLUMNS) ||
        (rows == 0u) || (rows > LCD_MAX_ROWS) ||
        (((unsigned int)columns * (unsigned int)rows) >
         LCD_DDRAM_CHARACTERS) ||
        ((char_size != LCD_5x8DOTS) && (char_size != LCD_5x10DOTS))) {
        return LIQUIDCRYSTAL_ERROR;
    }
    lcd_state.flags &= (uint8_t)~LCD_FLAG_INITIALIZED;

    lcd_state.columns = columns;
    lcd_state.rows = rows;
    LiquidCrystal_setRowOffsets(0x00, 0x40, columns, 0x40 + columns);
    lcd_state.display_function &= LCD_8BITMODE;
    if (rows > 1u) {
        lcd_state.display_function |= LCD_2LINE;
    } else if (char_size == LCD_5x10DOTS) {
        lcd_state.display_function |= LCD_5x10DOTS;
    }

    lcd_prepare_output_low(lcd_state.rs_pin);
    if (lcd_state.rw_pin != NOT_A_PIN) {
        lcd_prepare_output_low(lcd_state.rw_pin);
    }
    lcd_prepare_output_low(lcd_state.enable_pin);

    data_pin_count = ((lcd_state.display_function & LCD_8BITMODE) != 0u) ?
        8u : 4u;
    for (index = 0u; index < data_pin_count; ++index) {
        lcd_prepare_output_low(lcd_state.data_pins[index]);
    }

    delayMicroseconds(50000u);
    lcd_state.flags |= LCD_FLAG_INITIALIZED;

    if ((lcd_state.display_function & LCD_8BITMODE) == 0u) {
        lcd_write4bits(0x03u);
        delayMicroseconds(4500u);
        lcd_write4bits(0x03u);
        delayMicroseconds(4500u);
        lcd_write4bits(0x03u);
        delayMicroseconds(150u);
        lcd_write4bits(0x02u);
    } else {
        LiquidCrystal_command((uint8_t)(LCD_FUNCTIONSET |
                                        lcd_state.display_function));
        delayMicroseconds(4500u);
        LiquidCrystal_command((uint8_t)(LCD_FUNCTIONSET |
                                        lcd_state.display_function));
        delayMicroseconds(150u);
        LiquidCrystal_command((uint8_t)(LCD_FUNCTIONSET |
                                        lcd_state.display_function));
    }

    LiquidCrystal_command((uint8_t)(LCD_FUNCTIONSET |
                                    lcd_state.display_function));
    lcd_state.display_control = LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF;
    LiquidCrystal_display();
    LiquidCrystal_clear();
    lcd_state.display_mode = LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT;
    LiquidCrystal_command((uint8_t)(LCD_ENTRYMODESET |
                                    lcd_state.display_mode));
    return LIQUIDCRYSTAL_OK;
}

uint8_t LiquidCrystal_begin(uint8_t columns, uint8_t rows)
                            STC_LIQUIDCRYSTAL_REENTRANT
{
    return LiquidCrystal_beginWithCharSize(columns, rows, LCD_5x8DOTS);
}

void LiquidCrystal_clear(void) STC_LIQUIDCRYSTAL_REENTRANT
{
    if (lcd_is_ready() == 0u) {
        return;
    }
    LiquidCrystal_command(LCD_CLEARDISPLAY);
    delayMicroseconds(2000u);
}

void LiquidCrystal_home(void) STC_LIQUIDCRYSTAL_REENTRANT
{
    if (lcd_is_ready() == 0u) {
        return;
    }
    LiquidCrystal_command(LCD_RETURNHOME);
    delayMicroseconds(2000u);
}

void LiquidCrystal_setCursor(uint8_t column, uint8_t row)
                             STC_LIQUIDCRYSTAL_REENTRANT
{
    if (lcd_is_ready() == 0u) {
        return;
    }
    if (row >= lcd_state.rows) {
        row = (uint8_t)(lcd_state.rows - 1u);
    }

    LiquidCrystal_command((uint8_t)(LCD_SETDDRAMADDR |
                                    (column + lcd_state.row_offsets[row])));
}

void LiquidCrystal_setRowOffsets(int row0, int row1, int row2, int row3)
                                  STC_LIQUIDCRYSTAL_REENTRANT
{
    /* Match the public Arduino API's conversion to an 8-bit DDRAM offset. */
    lcd_state.row_offsets[0] = (uint8_t)row0;
    lcd_state.row_offsets[1] = (uint8_t)row1;
    lcd_state.row_offsets[2] = (uint8_t)row2;
    lcd_state.row_offsets[3] = (uint8_t)row3;
}

void LiquidCrystal_noDisplay(void) STC_LIQUIDCRYSTAL_REENTRANT
{
    if (lcd_is_ready() == 0u) {
        return;
    }
    lcd_state.display_control &= (uint8_t)~LCD_DISPLAYON;
    LiquidCrystal_command((uint8_t)(LCD_DISPLAYCONTROL |
                                    lcd_state.display_control));
}

void LiquidCrystal_display(void) STC_LIQUIDCRYSTAL_REENTRANT
{
    if (lcd_is_ready() == 0u) {
        return;
    }
    lcd_state.display_control |= LCD_DISPLAYON;
    LiquidCrystal_command((uint8_t)(LCD_DISPLAYCONTROL |
                                    lcd_state.display_control));
}

void LiquidCrystal_noCursor(void) STC_LIQUIDCRYSTAL_REENTRANT
{
    if (lcd_is_ready() == 0u) {
        return;
    }
    lcd_state.display_control &= (uint8_t)~LCD_CURSORON;
    LiquidCrystal_command((uint8_t)(LCD_DISPLAYCONTROL |
                                    lcd_state.display_control));
}

void LiquidCrystal_cursor(void) STC_LIQUIDCRYSTAL_REENTRANT
{
    if (lcd_is_ready() == 0u) {
        return;
    }
    lcd_state.display_control |= LCD_CURSORON;
    LiquidCrystal_command((uint8_t)(LCD_DISPLAYCONTROL |
                                    lcd_state.display_control));
}

void LiquidCrystal_noBlink(void) STC_LIQUIDCRYSTAL_REENTRANT
{
    if (lcd_is_ready() == 0u) {
        return;
    }
    lcd_state.display_control &= (uint8_t)~LCD_BLINKON;
    LiquidCrystal_command((uint8_t)(LCD_DISPLAYCONTROL |
                                    lcd_state.display_control));
}

void LiquidCrystal_blink(void) STC_LIQUIDCRYSTAL_REENTRANT
{
    if (lcd_is_ready() == 0u) {
        return;
    }
    lcd_state.display_control |= LCD_BLINKON;
    LiquidCrystal_command((uint8_t)(LCD_DISPLAYCONTROL |
                                    lcd_state.display_control));
}

void LiquidCrystal_scrollLeft(void) STC_LIQUIDCRYSTAL_REENTRANT
{
    LiquidCrystal_command(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVELEFT);
}

void LiquidCrystal_scrollRight(void) STC_LIQUIDCRYSTAL_REENTRANT
{
    LiquidCrystal_command(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVERIGHT);
}

void LiquidCrystal_scrollDisplayLeft(void) STC_LIQUIDCRYSTAL_REENTRANT
{
    LiquidCrystal_scrollLeft();
}

void LiquidCrystal_scrollDisplayRight(void) STC_LIQUIDCRYSTAL_REENTRANT
{
    LiquidCrystal_scrollRight();
}

void LiquidCrystal_leftToRight(void) STC_LIQUIDCRYSTAL_REENTRANT
{
    if (lcd_is_ready() == 0u) {
        return;
    }
    lcd_state.display_mode |= LCD_ENTRYLEFT;
    LiquidCrystal_command((uint8_t)(LCD_ENTRYMODESET | lcd_state.display_mode));
}

void LiquidCrystal_rightToLeft(void) STC_LIQUIDCRYSTAL_REENTRANT
{
    if (lcd_is_ready() == 0u) {
        return;
    }
    lcd_state.display_mode &= (uint8_t)~LCD_ENTRYLEFT;
    LiquidCrystal_command((uint8_t)(LCD_ENTRYMODESET | lcd_state.display_mode));
}

void LiquidCrystal_autoscroll(void) STC_LIQUIDCRYSTAL_REENTRANT
{
    if (lcd_is_ready() == 0u) {
        return;
    }
    lcd_state.display_mode |= LCD_ENTRYSHIFTINCREMENT;
    LiquidCrystal_command((uint8_t)(LCD_ENTRYMODESET | lcd_state.display_mode));
}

void LiquidCrystal_noAutoscroll(void) STC_LIQUIDCRYSTAL_REENTRANT
{
    if (lcd_is_ready() == 0u) {
        return;
    }
    lcd_state.display_mode &= (uint8_t)~LCD_ENTRYSHIFTINCREMENT;
    LiquidCrystal_command((uint8_t)(LCD_ENTRYMODESET | lcd_state.display_mode));
}

void LiquidCrystal_createChar(uint8_t location, const uint8_t charmap[8])
                              STC_LIQUIDCRYSTAL_REENTRANT
{
    uint8_t index;

    if ((lcd_is_ready() == 0u) || (charmap == (const uint8_t *)0)) {
        return;
    }
    location &= 0x07u;
    LiquidCrystal_command((uint8_t)(LCD_SETCGRAMADDR | (location << 3)));
    for (index = 0u; index < 8u; ++index) {
        (void)LiquidCrystal_write(charmap[index]);
    }
}

void LiquidCrystal_command(uint8_t value) STC_LIQUIDCRYSTAL_REENTRANT
{
    lcd_send(value, 0u);
}

size_t LiquidCrystal_write(uint8_t value) STC_LIQUIDCRYSTAL_REENTRANT
{
    if (lcd_is_ready() == 0u) {
        return 0u;
    }
    lcd_send(value, 1u);
    return 1u;
}
