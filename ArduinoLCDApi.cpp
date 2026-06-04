/**
 * @file    ArduinoLCDApi.cpp
 * @brief   Implementation of the LiquidCrystal LCDUI backend (diff renderer).
 *
 * Implements the off-screen frame buffer, the shadow comparison in flush_impl()
 * that pushes only changed cells to the display, and the integer-to-string and
 * cursor helpers used by the UI widgets. No dynamic allocation is used.
 *
 * @author  Jarosław Marek Niewiński (current version and modifications)
 * @author  Documentation and implementation assistance: Claude (Anthropic AI)
 * @note    Based on the VFO project by Jan Ciger (Janoc):
 *          https://janoc.rd-h.com/archives/649
 * @license GNU General Public License v3.0 (GPLv3)
 */

#include "ArduinoLCDApi.h"
#include <string.h>
#include <stdlib.h>

void TArduinoLCDApi::clear_impl()
{
    // Clears only the new_frame buffer - NOT the physical LCD
    // Physical LCD update happens in flush_impl()
    for (uint8_t r = 0; r < LCD_ROWS; r++)
        for (uint8_t c = 0; c < LCD_COLS; c++)
            m_new_frame[r][c] = ' ';
    m_cursor_x = 0;
    m_cursor_y = 0;
}

void TArduinoLCDApi::putchar_to_frame(char c)
{
    if (m_cursor_y < LCD_ROWS && m_cursor_x < LCD_COLS)
    {
        m_new_frame[m_cursor_y][m_cursor_x] = c;
        m_cursor_x++;
        if (m_cursor_x >= LCD_COLS)
        {
            m_cursor_x = 0;
            if (m_cursor_y < LCD_ROWS - 1)
                m_cursor_y++;
        }
    }
}

void TArduinoLCDApi::drawText_impl(const char *text)
{
    if (!text) return;
    while (*text)
        putchar_to_frame(*text++);
}

void TArduinoLCDApi::drawText_impl(int_fast32_t val)
{
    char buf[12];
    ltoa(val, buf, 10);
    drawText_impl(buf);
}

void TArduinoLCDApi::setCursorVisible_impl(bool visible)
{
    m_hw_cursor_visible = visible;
    // Save current logical cursor position as HW cursor pos
    m_hw_cursor_x = m_cursor_x;
    m_hw_cursor_y = m_cursor_y;
}

void TArduinoLCDApi::setBlink_impl(bool blink)
{
    m_hw_blink = blink;
}

void TArduinoLCDApi::setCursorPosition_impl(uint8_t x, uint8_t y)
{
    m_cursor_x = x;
    m_cursor_y = y;
    // Track last explicitly-set cursor position for HW cursor
    m_hw_cursor_x = x;
    m_hw_cursor_y = y;
}

// int2string_impl: no dynamic allocation, pure stack
void TArduinoLCDApi::int2string_impl(char *output, uint8_t max_len,
                                      int32_t num, bool pad)
{
    (void) pad;
    char tmp[12];
    ltoa(num, tmp, 10);

    uint8_t num_len = strlen(tmp);
    uint8_t out_len = max_len - 1; // leave room for null terminator

    uint8_t i = 0;
    // Left-pad with spaces
    while (i + num_len < out_len)
        output[i++] = ' ';
    // Copy number
    uint8_t j = 0;
    while (j < num_len && i < out_len)
        output[i++] = tmp[j++];
    output[i] = '\0';
}

// flush_impl: diff-render - only sends changed characters to LCD
// This eliminates clear() flicker and minimises I2C/parallel bus traffic
void TArduinoLCDApi::flush_impl()
{
    uint8_t last_row = 0xFF;
    uint8_t last_col = 0xFF;

    for (uint8_t r = 0; r < LCD_ROWS; r++)
    {
        for (uint8_t c = 0; c < LCD_COLS; c++)
        {
            char nc = m_new_frame[r][c];
            if (nc != m_shadow[r][c])
            {
                // Only call setCursor when we can't rely on auto-advance
                bool need_set = (r != last_row) || (c != last_col);
                if (need_set)
                    m_lcd.setCursor(c, r);

                m_lcd.write(nc);
                m_shadow[r][c] = nc;

                last_row = r;
                last_col = c + 1; // LCD auto-advances column after write
            }
            else
            {
                // Dirty gap: next write must explicitly reposition
                last_row = 0xFF;
                last_col = 0xFF;
            }
        }
    }

    // Apply hardware cursor
    if (m_hw_cursor_visible)
    {
        m_lcd.setCursor(m_hw_cursor_x, m_hw_cursor_y);
        m_lcd.cursor();
    }
    else
    {
        m_lcd.noCursor();
    }

    if (m_hw_blink)
        m_lcd.blink();
    else
        m_lcd.noBlink();
}
