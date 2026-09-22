/*
 * SPDX-License-Identifier: MIT
 *
 * Arduino-compatible C++ facade for the STC HD44780 HAL.
 */
#ifndef STC_LIQUID_CRYSTAL_CLASS_H
#define STC_LIQUID_CRYSTAL_CLASS_H

class LiquidCrystal : public Print
{
public:
    LiquidCrystal(uint8_t rs, uint8_t enable,
                  uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7)
        : _state(), _configured(false)
    {
        Context context(&_state);
        _configured = LiquidCrystal_setPins(rs, enable, d4, d5, d6, d7) == LIQUIDCRYSTAL_OK;
    }

    LiquidCrystal(uint8_t rs, uint8_t rw, uint8_t enable,
                  uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7)
        : _state(), _configured(false)
    {
        Context context(&_state);
        _configured = LiquidCrystal_setPinsRW(rs, rw, enable, d4, d5, d6, d7) == LIQUIDCRYSTAL_OK;
    }

    LiquidCrystal(uint8_t rs, uint8_t enable,
                  uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3,
                  uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7)
        : _state(), _configured(false)
    {
        Context context(&_state);
        _configured = LiquidCrystal_setPins8(rs, enable, d0, d1, d2, d3, d4, d5, d6, d7) == LIQUIDCRYSTAL_OK;
    }

    LiquidCrystal(uint8_t rs, uint8_t rw, uint8_t enable,
                  uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3,
                  uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7)
        : _state(), _configured(false)
    {
        Context context(&_state);
        _configured = LiquidCrystal_setPins8RW(rs, rw, enable, d0, d1, d2, d3, d4, d5, d6, d7) == LIQUIDCRYSTAL_OK;
    }

    void init(uint8_t fourbitmode, uint8_t rs, uint8_t rw, uint8_t enable,
              uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3,
              uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7)
    {
        Context context(&_state);
        uint8_t configured = fourbitmode
            ? LiquidCrystal_setPinsRW(rs, rw, enable, d0, d1, d2, d3)
            : LiquidCrystal_setPins8RW(rs, rw, enable,
                                      d0, d1, d2, d3, d4, d5, d6, d7);
        _configured = configured == LIQUIDCRYSTAL_OK &&
                      LiquidCrystal_begin(16u, 1u) == LIQUIDCRYSTAL_OK;
    }

    void begin(uint8_t columns, uint8_t rows,
               uint8_t charSize = LCD_5x8DOTS)
    {
        Context context(&_state);
        _configured = LiquidCrystal_beginWithCharSize(columns, rows, charSize) == LIQUIDCRYSTAL_OK;
    }

    void clear() { Context context(&_state); LiquidCrystal_clear(); }
    void home() { Context context(&_state); LiquidCrystal_home(); }
    void noDisplay() { Context context(&_state); LiquidCrystal_noDisplay(); }
    void display() { Context context(&_state); LiquidCrystal_display(); }
    void noBlink() { Context context(&_state); LiquidCrystal_noBlink(); }
    void blink() { Context context(&_state); LiquidCrystal_blink(); }
    void noCursor() { Context context(&_state); LiquidCrystal_noCursor(); }
    void cursor() { Context context(&_state); LiquidCrystal_cursor(); }
    void scrollDisplayLeft() { Context context(&_state); LiquidCrystal_scrollDisplayLeft(); }
    void scrollDisplayRight() { Context context(&_state); LiquidCrystal_scrollDisplayRight(); }
    void leftToRight() { Context context(&_state); LiquidCrystal_leftToRight(); }
    void rightToLeft() { Context context(&_state); LiquidCrystal_rightToLeft(); }
    void autoscroll() { Context context(&_state); LiquidCrystal_autoscroll(); }
    void noAutoscroll() { Context context(&_state); LiquidCrystal_noAutoscroll(); }
    void setCursor(uint8_t column, uint8_t row)
    {
        Context context(&_state);
        LiquidCrystal_setCursor(column, row);
    }
    void setRowOffsets(int row0, int row1, int row2, int row3)
    {
        Context context(&_state);
        LiquidCrystal_setRowOffsets(row0, row1, row2, row3);
    }
    void createChar(uint8_t location, uint8_t charmap[])
    {
        Context context(&_state);
        LiquidCrystal_createChar(location, charmap);
    }
    void command(uint8_t value) { Context context(&_state); LiquidCrystal_command(value); }

    size_t write(uint8_t value) override
    {
        Context context(&_state);
        size_t written = LiquidCrystal_write(value);
        if (written != 1u) { setWriteError(); }
        return written;
    }
    using Print::write;

    explicit operator bool() const { return _configured; }

private:
    class Context {
        STCLiquidCrystalState *_previous;
    public:
        explicit Context(STCLiquidCrystalState *state) : _previous(LiquidCrystal_selectContext(state)) {}
        ~Context() { LiquidCrystal_selectContext(_previous); }
        Context(const Context &) = delete;
        Context &operator=(const Context &) = delete;
    };
    STCLiquidCrystalState _state;
    bool _configured;
};

#endif
