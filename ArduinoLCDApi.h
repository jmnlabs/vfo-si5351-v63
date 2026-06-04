/**
 * @file    ArduinoLCDApi.h
 * @brief   LiquidCrystal backend for the LCDUI framework with diff rendering.
 *
 * Concrete LCDUI::LCDApiBase implementation driving a 16x2 HD44780 display via
 * the Arduino LiquidCrystal library. Drawing operations are accumulated into
 * an off-screen frame buffer; flush() then compares it against a shadow copy
 * and writes only the characters that actually changed. This eliminates the
 * flicker of full-screen clears and minimises bus traffic to the display.
 *
 * @author  Jarosław Marek Niewiński (current version and modifications)
 * @author  Documentation and implementation assistance: Claude (Anthropic AI)
 * @note    Based on the VFO project by Jan Ciger (Janoc):
 *          https://janoc.rd-h.com/archives/649
 * @license GNU General Public License v3.0 (GPLv3)
 */

#ifndef ARDUINOLCDAPI_H
#define ARDUINOLCDAPI_H

#include <LiquidCrystal.h>
#include "lcdui/lcdui.h"

class TArduinoLCDApi : public LCDUI::LCDApiBase<TArduinoLCDApi>
{
    using base = LCDUI::LCDApiBase<TArduinoLCDApi>;
public:
    static constexpr uint8_t LCD_COLS = 16;
    static constexpr uint8_t LCD_ROWS = 2;

    TArduinoLCDApi(LiquidCrystal &display, uint8_t width, uint8_t height)
        : base(width, height)
        , m_lcd(display)
    {
        for (uint8_t r = 0; r < LCD_ROWS; r++)
            for (uint8_t c = 0; c < LCD_COLS; c++)
            {
                m_new_frame[r][c] = ' ';
                m_shadow[r][c]    = '\0'; // force first full paint
            }
        m_cursor_x = 0;
        m_cursor_y = 0;
        m_hw_cursor_visible = false;
        m_hw_blink = false;
    }

    void clear_impl();
    void drawText_impl(const char *text);
    void drawText_impl(int_fast32_t val);
    void setCursorVisible_impl(bool visible);
    void setBlink_impl(bool blink);
    void setCursorPosition_impl(uint8_t x, uint8_t y);
    void int2string_impl(char *output, uint8_t max_len,
                         int32_t num, bool pad = false);
    // Called once per frame to push changes to LCD hardware
    void flush_impl();

private:
    LiquidCrystal &m_lcd;

    char m_new_frame[LCD_ROWS][LCD_COLS]; // what we want to show
    char m_shadow[LCD_ROWS][LCD_COLS];    // what is on screen now

    uint8_t m_cursor_x;
    uint8_t m_cursor_y;
    bool m_hw_cursor_visible;
    bool m_hw_blink;

    // cursor position for HW cursor (set by setCursorPosition)
    uint8_t m_hw_cursor_x;
    uint8_t m_hw_cursor_y;

    void putchar_to_frame(char c);
};

#endif /* ARDUINOLCDAPI_H */
