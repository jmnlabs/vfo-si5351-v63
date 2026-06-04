/**
 * @file    lcdapi.h
 * @brief   LCD abstraction base class and the UI input event.
 *
 * Declares LCDUI::InputEvent (encoder delta plus the function-button bitfield)
 * and LCDUI::LCDApiBase, a CRTP base that defines the drawing/cursor interface
 * the widgets call. Concrete displays derive from it (see TArduinoLCDApi),
 * keeping the UI code independent of any particular LCD library.
 *
 * @author  Jarosław Marek Niewiński (current version and modifications)
 * @author  Documentation and implementation assistance: Claude (Anthropic AI)
 * @note    Based on the VFO project by Jan Ciger (Janoc):
 *          https://janoc.rd-h.com/archives/649
 * @license GNU General Public License v3.0 (GPLv3)
 */

#ifndef LCDUI_LCDAPI_H_
#define LCDUI_LCDAPI_H_

#include <stdint.h>

namespace LCDUI
{
// InputEvent /////////////////////////////////////////////////////////////
struct InputEvent
{
    int ax, ay;

    union
    {
        struct
        {
            bool b_OK : 1;
            bool b_A : 1;
            bool b_B : 1;
            bool b_C : 1;
            bool b_D : 1;

            uint8_t b5 : 1;
            uint8_t b6 : 1;
            uint8_t b7 : 1;
        };

        uint8_t buttons;
    };
};

// LCDApi /////////////////////////////////////////////////////////////////
template <typename Derived> class LCDApiBase
{
public:
    LCDApiBase(uint8_t width, uint8_t height) : m_width(width), m_height(height){};

    inline void clear() { static_cast<Derived *>(this)->clear_impl(); }
    inline void drawText(const char *text) { static_cast<Derived *>(this)->drawText_impl(text); }
    inline void drawText(int_fast32_t val) { static_cast<Derived *>(this)->drawText_impl(val); }
    inline void setCursorVisible(bool visible)
    {
        static_cast<Derived *>(this)->setCursorVisible_impl(visible);
    }
    inline void setBlink(bool blink) { static_cast<Derived *>(this)->setBlink_impl(blink); }
    inline void setCursorPosition(uint8_t x, uint8_t y)
    {
        static_cast<Derived *>(this)->setCursorPosition_impl(x, y);
    }
    inline void int2string(char *output, uint8_t max_len, int32_t num, bool pad = false)
    {
        static_cast<Derived *>(this)->int2string_impl(output, max_len, num, pad);
    }
    // Flush shadow buffer to physical LCD - call once per render cycle
    inline void flush()
    {
        static_cast<Derived *>(this)->flush_impl();
    }

    inline uint8_t width() { return m_width; }
    inline uint8_t height() { return m_height; }

protected:
    uint8_t m_width, m_height;
};
}

#endif
