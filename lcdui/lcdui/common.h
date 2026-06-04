/**
 * @file    common.h
 * @brief   Shared UI primitives: the menu-item base class and null callback.
 *
 * Declares LCDUI::MenuItemBase, the CRTP base shared by every menu widget
 * (title, focus state, dirty/repaint tracking and the render/input dispatch),
 * and LCDUI::NullCallback used as the default no-op change handler.
 *
 * @author  Jarosław Marek Niewiński (current version and modifications)
 * @author  Documentation and implementation assistance: Claude (Anthropic AI)
 * @note    Based on the VFO project by Jan Ciger (Janoc):
 *          https://janoc.rd-h.com/archives/649
 * @license GNU General Public License v3.0 (GPLv3)
 */

#ifndef LCDUI_COMMON_H_
#define LCDUI_COMMON_H_

#include "lcdapi.h"

namespace LCDUI
{
// Empty callback /////////////////////////////////////////////////////////
struct TNullCallback
{
    template <typename T> void operator()(T &&v) {}
};
static TNullCallback NullCallback;

// MenuItem ///////////////////////////////////////////////////////////////////
template <typename LCDApi, typename Derived> class MenuItemBase
{
public:
    MenuItemBase(LCDApi &api, const char *title)
        : m_lcd_api(api), m_title(title), m_focus(false), m_dirty(false)
    {
    }

    void render()
    {
        static_cast<Derived *>(this)->render_impl();
        m_dirty = false;
    }
    void handle_input(const InputEvent &ev) { static_cast<Derived *>(this)->handle_input_impl(ev); }

    constexpr inline bool is_focused() const { return m_focus; }
    inline void set_focus(bool focus)
    {
        static_cast<Derived *>(this)->set_focus_impl(focus);
        static_cast<Derived *>(this)->repaint_impl();
    }

    constexpr inline bool is_dirty() const { return m_dirty; }
    inline void repaint() { static_cast<Derived *>(this)->repaint_impl(); }

    // default implementations
    inline void render_impl() {}
    inline void handle_input_impl(const InputEvent &ev) {}
    inline void set_focus_impl(bool focus) { m_focus = focus; }
    inline void repaint_impl() { m_dirty = true; }

protected:
    LCDApi &m_lcd_api;
    const char *m_title;
    bool m_focus;
    bool m_dirty;
};
}

#endif
