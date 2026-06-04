/**
 * @file    number.h
 * @brief   Decade (digit-by-digit) numeric editor widget.
 *
 * LCDUI::MenuNumber edits an integer one decade at a time: the encoder first
 * selects a digit position, then adjusts that digit in 1/10/100/... steps,
 * clamped to a [min, max] range. Used for the IF1, IF2, BFO-centre and Si5351
 * calibration menu items. make_number() factory helpers support lambdas.
 *
 * @author  Jarosław Marek Niewiński (current version and modifications)
 * @author  Documentation and implementation assistance: Claude (Anthropic AI)
 * @note    Based on the VFO project by Jan Ciger (Janoc):
 *          https://janoc.rd-h.com/archives/649
 * @license GNU General Public License v3.0 (GPLv3)
 */

#ifndef LCDUI_NUMBER_H_
#define LCDUI_NUMBER_H_

#include "common.h"

namespace LCDUI
{
// MenuNumber ///////////////////////////////////////////////////////////
template <typename LCDApi, typename TCallback, typename TData>
class MenuNumber : public MenuItemBase<LCDApi, MenuNumber<LCDApi, TCallback, TData>>
{
    using base = MenuItemBase<LCDApi, MenuNumber<LCDApi, TCallback, TData>>;

public:
    MenuNumber(LCDApi &api, const char *title, uint8_t width, TData &value, TCallback &on_changed_cb,
               TData min = 0, TData max = 255)
        : base(api, title)
        , m_width(width)
        , m_value(value)
        , m_onchanged_cb(on_changed_cb)
        , m_min(min)
        , m_max(max)
        , m_selected_decade(width - 1)
        , m_editing(false)
    {
    }

    MenuNumber(LCDApi &api, const char *title, uint8_t width, TData &value, TData min = 0, TData max = 255)
        : base(api, title)
        , m_width(width)
        , m_value(value)
        , m_min(min)
        , m_max(max)
        , m_selected_decade(width - 1)
        , m_editing(false)
        , m_onchanged_cb(NullCallback)
    {
    }

    void render_impl()
    {
        if (!this->is_dirty())
            return;

        this->m_lcd_api.clear();

        this->m_lcd_api.setCursorPosition(0, 0);
        this->m_lcd_api.drawText(this->m_title);

        this->m_lcd_api.setCursorPosition(0, 1);

        char buf[m_width + 1];
        this->m_lcd_api.int2string(buf, m_width + 1, m_value);

        auto fc = this->is_focused();
        if (fc)
            this->m_lcd_api.drawText(">");
        else
            this->m_lcd_api.drawText(" ");

        this->m_lcd_api.drawText(buf);

        if (fc)
            this->m_lcd_api.drawText("<");

        this->m_lcd_api.setCursorPosition(m_width - m_selected_decade, 1);
        if (fc)
        {
            if (m_editing)
            {
                this->m_lcd_api.setCursorVisible(true);
            } else
            {
                this->m_lcd_api.setCursorVisible(false);
                this->m_lcd_api.drawText("?");
            }
        } else
            this->m_lcd_api.setCursorVisible(false);
    }

    void handle_input_impl(const InputEvent &ev)
    {
        if (m_editing && m_selected_decade < m_width - 1)
        {
            TData increment = 1;
            for (uint8_t i = 0; i < m_selected_decade; ++i)
                increment *= 10;

            if (ev.ay < 0)
            {
                if (m_value - increment >= m_min)
                    m_value -= increment;
                else
                    m_value = m_min;

                m_onchanged_cb(m_value);

                this->repaint();
            }

            else if (ev.ay > 0)
            {
                if (m_value + increment <= m_max)
                    m_value += increment;
                else
                    m_value = m_max;

                m_onchanged_cb(m_value);

                this->repaint();
            }

            else
            {
            }
        }

        else
        {
            if (ev.ay > 0 && m_selected_decade > 0)
            {
                --m_selected_decade;
                this->repaint();
            }

            else if (ev.ay < 0 && m_selected_decade < m_width - 1)
            {
                ++m_selected_decade;
                this->repaint();
            }
        }

        if (ev.b_OK)
        {
            if (m_selected_decade == m_width - 1)
            {
                this->set_focus(false);
                m_selected_decade = m_width - 1;
            } else
                m_editing = !m_editing;

            this->repaint();
        }
    }

protected:
    uint8_t m_width;
    TData &m_value;
    TCallback &m_onchanged_cb;
    TData m_min, m_max;

    int8_t m_selected_decade;
    bool m_editing;
};

template <typename LCDApi, typename TCallback, typename TData>
constexpr MenuNumber<LCDApi, TCallback, TData> make_number(LCDApi &api, const char *title, uint8_t width,
                                                           TData &value, TCallback &&on_changed_cb,
                                                           TData min = 0, TData max = 255)
{
    return MenuNumber<LCDApi, TCallback, TData>(api, title, width, value, on_changed_cb, min, max);
}

template <typename LCDApi, typename TCallback, typename TData>
constexpr MenuNumber<LCDApi, TCallback, TData> make_number(LCDApi &api, const char *title, uint8_t width,
                                                           TData &value, TData min = 0, TData max = 255)
{
    return MenuNumber<LCDApi, TCallback, TData>(api, title, width, value, min, max);
}
}

#endif
