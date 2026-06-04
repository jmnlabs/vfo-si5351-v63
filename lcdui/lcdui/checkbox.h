/**
 * @file    checkbox.h
 * @brief   Boolean checkbox menu widget.
 *
 * LCDUI::MenuCheckbox toggles a bound bool and invokes an optional callback on
 * change. It renders "[ ]"/"[x]" when unfocused and "( )"/"(x)" when focused.
 * make_checkbox() factory helpers allow lambdas to be used as callbacks.
 *
 * @author  Jarosław Marek Niewiński (current version and modifications)
 * @author  Documentation and implementation assistance: Claude (Anthropic AI)
 * @note    Based on the VFO project by Jan Ciger (Janoc):
 *          https://janoc.rd-h.com/archives/649
 * @license GNU General Public License v3.0 (GPLv3)
 */

#ifndef LCDUI_CHECKBOX_H_
#define LCDUI_CHECKBOX_H_

#include "common.h"

namespace LCDUI
{
// MenuCheckbox ///////////////////////////////////////////////////////////
template <typename LCDApi, typename TCallback> class MenuCheckbox : public MenuItemBase<LCDApi, MenuCheckbox<LCDApi, TCallback>>
{
    using base = MenuItemBase<LCDApi, MenuCheckbox<LCDApi, TCallback>>;

public:
    MenuCheckbox(LCDApi &api, const char *title, bool &checked)
        : base(api, title), m_checked(checked), m_onchanged_cb(NullCallback)
    {
    }

    MenuCheckbox(LCDApi &api, const char *title, bool &checked, TCallback &on_change_cb)
        : base(api, title), m_checked(checked), m_onchanged_cb(on_change_cb)
    {
    }

    constexpr inline bool checked() const { return m_checked; };

    void render_impl()
    {
        if (!this->is_dirty())
            return;

        this->m_lcd_api.clear();
        this->m_lcd_api.setCursorPosition(0, 0);
        this->m_lcd_api.drawText(this->m_title);

        this->m_lcd_api.setCursorPosition(0, 1);

        if (this->is_focused())
            this->m_lcd_api.drawText(m_checked ? "(x)" : "( )");
        else
            this->m_lcd_api.drawText(m_checked ? "[x]" : "[ ]");
    }

    void handle_input_impl(const InputEvent &ev)
    {
        if (ev.ay != 0)
        {
            this->m_focus = false;
            this->repaint();
        }

        else if (ev.b_OK)
        {
            m_checked = !m_checked;
            m_onchanged_cb(m_checked);

            this->repaint();
        }
    }

    void set_focus_impl(bool focus)
    {
        this->m_focus = focus;

        m_checked = !m_checked;
        m_onchanged_cb(m_checked);
    }

protected:
    bool &m_checked;
    TCallback &m_onchanged_cb;
};

// Factory functions to permit using lambdas as callbacks without having to provide template parameters.
// Automatic type deduction doesn't work for class constructors yet :(
template <typename LCDApi, typename TCallback = TNullCallback>
constexpr MenuCheckbox<LCDApi, TCallback> make_checkbox(LCDApi &api, const char *title, bool &checked)
{
    return MenuCheckbox<LCDApi, TCallback>(api, title, checked);
}

template <typename LCDApi, typename TCallback>
constexpr MenuCheckbox<LCDApi, TCallback> make_checkbox(LCDApi &api, const char *title, bool &checked, TCallback &&on_change_cb)
{
    return MenuCheckbox<LCDApi, TCallback>(api, title, checked, on_change_cb);
}
}

#endif
