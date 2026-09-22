/* SPDX-License-Identifier: MIT */
/* Internal C hardware backend for the Arduino C++ library. */
#ifndef STC_LIQUIDCRYSTAL_BACKEND_H
#define STC_LIQUIDCRYSTAL_BACKEND_H

#include <Arduino.h>

#ifdef __cplusplus
extern "C" {
#endif

/* SDCC indirect calls cannot use callee-specific overlay parameter slots. */
#if defined(__SDCC)
# define STC_LIQUIDCRYSTAL_REENTRANT __reentrant
#else
# define STC_LIQUIDCRYSTAL_REENTRANT
#endif

/* Commands. */
#define LCD_CLEARDISPLAY   0x01u
#define LCD_RETURNHOME     0x02u
#define LCD_ENTRYMODESET   0x04u
#define LCD_DISPLAYCONTROL 0x08u
#define LCD_CURSORSHIFT    0x10u
#define LCD_FUNCTIONSET    0x20u
#define LCD_SETCGRAMADDR   0x40u
#define LCD_SETDDRAMADDR   0x80u

/* Entry-mode flags. */
#define LCD_ENTRYRIGHT          0x00u
#define LCD_ENTRYLEFT           0x02u
#define LCD_ENTRYSHIFTINCREMENT 0x01u
#define LCD_ENTRYSHIFTDECREMENT 0x00u

/* Display-control flags. */
#define LCD_DISPLAYON  0x04u
#define LCD_DISPLAYOFF 0x00u
#define LCD_CURSORON   0x02u
#define LCD_CURSOROFF  0x00u
#define LCD_BLINKON    0x01u
#define LCD_BLINKOFF   0x00u

/* Cursor/display-shift flags. */
#define LCD_DISPLAYMOVE 0x08u
#define LCD_CURSORMOVE  0x00u
#define LCD_MOVERIGHT   0x04u
#define LCD_MOVELEFT    0x00u

/* Function-set flags. */
#define LCD_8BITMODE 0x10u
#define LCD_4BITMODE 0x00u
#define LCD_2LINE    0x08u
#define LCD_1LINE    0x00u
#define LCD_5x10DOTS 0x04u
#define LCD_5x8DOTS  0x00u

#define LIQUIDCRYSTAL_ERROR 0u
#define LIQUIDCRYSTAL_OK    1u

typedef struct {
    uint8_t rs_pin, rw_pin, enable_pin;
    uint8_t data_pins[8];
    uint8_t columns, rows, display_function, display_control, display_mode, flags;
    uint8_t row_offsets[4];
} STCLiquidCrystalState;
STCLiquidCrystalState *LiquidCrystal_selectContext(STCLiquidCrystalState *context)
    STC_LIQUIDCRYSTAL_REENTRANT;

/* Common write-only wiring, with the controller R/W pin tied to ground. */
uint8_t LiquidCrystal_setPins(uint8_t rs_pin, uint8_t enable_pin,
                              uint8_t d4_pin, uint8_t d5_pin,
                              uint8_t d6_pin,
                              uint8_t d7_pin) STC_LIQUIDCRYSTAL_REENTRANT;

/* Optional R/W wiring.  The driver still uses fixed delays, not busy reads. */
uint8_t LiquidCrystal_setPinsRW(uint8_t rs_pin, uint8_t rw_pin,
                                uint8_t enable_pin, uint8_t d4_pin,
                                uint8_t d5_pin, uint8_t d6_pin,
                                uint8_t d7_pin)
                                STC_LIQUIDCRYSTAL_REENTRANT;

uint8_t LiquidCrystal_setPins8(uint8_t rs_pin, uint8_t enable_pin,
                               uint8_t d0_pin, uint8_t d1_pin,
                               uint8_t d2_pin, uint8_t d3_pin,
                               uint8_t d4_pin, uint8_t d5_pin,
                               uint8_t d6_pin,
                               uint8_t d7_pin) STC_LIQUIDCRYSTAL_REENTRANT;

uint8_t LiquidCrystal_setPins8RW(uint8_t rs_pin, uint8_t rw_pin,
                                 uint8_t enable_pin, uint8_t d0_pin,
                                 uint8_t d1_pin, uint8_t d2_pin,
                                 uint8_t d3_pin, uint8_t d4_pin,
                                 uint8_t d5_pin, uint8_t d6_pin,
                                 uint8_t d7_pin)
                                 STC_LIQUIDCRYSTAL_REENTRANT;

uint8_t LiquidCrystal_begin(uint8_t columns,
                            uint8_t rows) STC_LIQUIDCRYSTAL_REENTRANT;
uint8_t LiquidCrystal_beginWithCharSize(uint8_t columns, uint8_t rows,
                                        uint8_t char_size)
                                        STC_LIQUIDCRYSTAL_REENTRANT;
void LiquidCrystal_clear(void) STC_LIQUIDCRYSTAL_REENTRANT;
void LiquidCrystal_home(void) STC_LIQUIDCRYSTAL_REENTRANT;
void LiquidCrystal_setCursor(uint8_t column,
                             uint8_t row) STC_LIQUIDCRYSTAL_REENTRANT;
void LiquidCrystal_setRowOffsets(int row0, int row1, int row2, int row3)
                                  STC_LIQUIDCRYSTAL_REENTRANT;
void LiquidCrystal_display(void) STC_LIQUIDCRYSTAL_REENTRANT;
void LiquidCrystal_noDisplay(void) STC_LIQUIDCRYSTAL_REENTRANT;
void LiquidCrystal_cursor(void) STC_LIQUIDCRYSTAL_REENTRANT;
void LiquidCrystal_noCursor(void) STC_LIQUIDCRYSTAL_REENTRANT;
void LiquidCrystal_blink(void) STC_LIQUIDCRYSTAL_REENTRANT;
void LiquidCrystal_noBlink(void) STC_LIQUIDCRYSTAL_REENTRANT;
void LiquidCrystal_scrollLeft(void) STC_LIQUIDCRYSTAL_REENTRANT;
void LiquidCrystal_scrollRight(void) STC_LIQUIDCRYSTAL_REENTRANT;
void LiquidCrystal_scrollDisplayLeft(void) STC_LIQUIDCRYSTAL_REENTRANT;
void LiquidCrystal_scrollDisplayRight(void) STC_LIQUIDCRYSTAL_REENTRANT;
void LiquidCrystal_leftToRight(void) STC_LIQUIDCRYSTAL_REENTRANT;
void LiquidCrystal_rightToLeft(void) STC_LIQUIDCRYSTAL_REENTRANT;
void LiquidCrystal_autoscroll(void) STC_LIQUIDCRYSTAL_REENTRANT;
void LiquidCrystal_noAutoscroll(void) STC_LIQUIDCRYSTAL_REENTRANT;
void LiquidCrystal_createChar(uint8_t location, const uint8_t charmap[8])
                              STC_LIQUIDCRYSTAL_REENTRANT;
void LiquidCrystal_command(uint8_t value) STC_LIQUIDCRYSTAL_REENTRANT;
size_t LiquidCrystal_write(uint8_t value) STC_LIQUIDCRYSTAL_REENTRANT;

#ifdef __cplusplus
}
#endif

#endif
