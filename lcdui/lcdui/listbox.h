/**
 * @file    listbox.h
 * @brief   Single-choice list menu widget.
 *
 * LCDUI::MenuListBox lets the user scroll through a fixed array of string
 * options with the encoder and reports the selected index/text through an
 * optional callback. Used for the operating mode, conversion type and 1st-LO
 * side menu items. make_listbox() factory helpers support lambda callbacks.
 *
 * @author  Jarosław Marek Niewiński (current version and modifications)
 * @author  Documentation and implementation assistance: Claude (Anthropic AI)
 * @note    Based on the VFO project by Jan Ciger (Janoc):
 *          https://janoc.rd-h.com/archives/649
 * @license GNU General Public License v3.0 (GPLv3)
 */

#ifndef LCDUI_LISTBOX_H_
#define LCDUI_LISTBOX_H_

#include "common.h"

namespace LCDUI
{
// MenuListBox ////////////////////////////////////////////////////////////
template <typename ArrayType, typename LCDApi, typename TCallback>
class MenuListBox : public MenuItemBase<LCDApi, MenuListBox<ArrayType, LCDApi, TCallback>>
{
    using base = MenuItemBase<LCDApi, MenuListBox<ArrayType, LCDApi, TCallback>>;

public:
    MenuListBox(LCDApi &api, const char *title, const ArrayType &values, TCallback &on_changed_cb,
                const uint8_t selected = 0)
        : base(api, title), m_selected(selected), m_values(values), m_onchanged_cb(on_changed_cb)
    {
    }

    MenuListBox(LCDApi &api, const char *title, const ArrayType &values, const uint8_t selected = 0)
        : base(api, title), m_selected(selected), m_values(values), m_onchanged_cb(NullCallback)
    {
    }

    constexpr inline uint8_t selected() const { return m_selected; }
    constexpr inline const char *selected_str() const { return m_values[m_selected]; }

    void render_impl()
    {
        if (!this->is_dirty())
            return;

        this->m_lcd_api.clear();
        this->m_lcd_api.setCursorPosition(0, 0);
        this->m_lcd_api.drawText(this->m_title);

        this->m_lcd_api.setCursorPosition(0, 1);

        if (this->is_focused())
            this->m_lcd_api.drawText("*");
        else
            this->m_lcd_api.drawText(" ");

        this->m_lcd_api.drawText(m_values[m_selected]);
    }

    void handle_input_impl(const InputEvent &ev)
    {
        if (ev.b_OK)
        {
            this->m_focus = false;
            this->repaint();
        }

        else if (ev.ay > 0 && m_selected < m_values.size() - 1)
        {
            ++m_selected;
            m_onchanged_cb(m_selected, m_values[m_selected]);

            this->repaint();
        }

        else if (ev.ay < 0 && m_selected > 0)
        {
            --m_selected;
            m_onchanged_cb(m_selected, m_values[m_selected]);

            this->repaint();
        }
    }

protected:
    uint8_t m_selected;
    const ArrayType &m_values;
    TCallback &m_onchanged_cb;
};

template <typename ArrayType, typename LCDApi, typename TCallback>
constexpr MenuListBox<ArrayType, LCDApi, TCallback>
make_listbox(LCDApi &api, const char *title, const ArrayType &values, TCallback &&changed_cb,
             const uint8_t selected = 0)
{
    return MenuListBox<ArrayType, LCDApi, TCallback>(api, title, values, changed_cb, selected);
}

template <typename ArrayType, typename LCDApi, typename TCallback>
constexpr MenuListBox<ArrayType, LCDApi, TCallback>
make_listbox(LCDApi &api, const char *title, const ArrayType &values, const uint8_t selected = 0)
{
    return MenuListBox<ArrayType, LCDApi, TCallback>(api, title, values, selected);
}
}

#endif
