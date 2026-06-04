/**
 * @file    menu.h
 * @brief   Static menu container holding a heterogeneous set of widgets.
 *
 * LCDUI::Menu stores its items in a compile-time tuple, so any mix of widget
 * types can be combined without virtual dispatch or heap allocation. It tracks
 * the current item, forwards input to it (entering/leaving focus as needed)
 * and scrolls between items with the encoder. make_menu() builds one with
 * deduced types.
 *
 * @author  Jarosław Marek Niewiński (current version and modifications)
 * @author  Documentation and implementation assistance: Claude (Anthropic AI)
 * @note    Based on the VFO project by Jan Ciger (Janoc):
 *          https://janoc.rd-h.com/archives/649
 * @license GNU General Public License v3.0 (GPLv3)
 */

#ifndef LCDUI_MENU_H_
#define LCDUI_MENU_H_

#include "common.h"
#include "utilities.h"

namespace LCDUI
{
// Menu ///////////////////////////////////////////////////////////////////
template <typename... Items> class Menu
{
public:
    explicit Menu(Items &&... items)
        : m_items(items...)
        , m_current_item_idx(0)
    {}

    void render(bool force = false)
    {
        if (m_current_item_idx == m_items.size)
            return;

        if (force)
            Utilities::visit_ith(m_current_item_idx, RepaintVisitor(), m_items);

        Utilities::visit_ith(m_current_item_idx, RenderVisitor(), m_items);
    }

    void handle_input(const InputEvent &ev)
    {
        if (m_current_item_idx < m_items.size)
        {
            // process input on the current item if focused, otherwise focus it
            auto input_visitor = HandleInputVisitor(ev);
            Utilities::visit_ith(m_current_item_idx, input_visitor, m_items);

            if (!input_visitor.is_focused())
            {
                if (ev.ay < 0 && m_current_item_idx > 0)
                    Utilities::visit_ith(--m_current_item_idx, RepaintVisitor(), m_items);
                else if (ev.ay > 0 && m_current_item_idx < m_items.size - 1)
                    Utilities::visit_ith(++m_current_item_idx, RepaintVisitor(), m_items);
            }
        }
    }

    unsigned current_idx() { return m_current_item_idx; }
    void set_current_idx(unsigned idx) { m_current_item_idx = idx; }

protected:
    struct HandleInputVisitor
    {
        explicit HandleInputVisitor(const InputEvent &ev) : m_ev(ev), m_focused(false) {}

        template <typename T> void operator()(T &&v)
        {
            m_focused = v.is_focused();

            if (v.is_focused())
                v.handle_input(m_ev);
            else if (m_ev.b_OK)
                v.set_focus(true);
            else
            // nothing
            {
            }
        }

        bool is_focused() { return m_focused; }

    private:
        const InputEvent &m_ev;
        bool m_focused;
    };

    struct RepaintVisitor
    {
        template <typename T> void operator()(T &&v) { v.repaint(); }
    };

    struct RenderVisitor
    {
        template <typename T> void operator()(T &&v) { v.render(); }
    };

protected:
    Utilities::Tuple<Items...> m_items;
    unsigned m_current_item_idx;
};

template <typename... Items> constexpr Menu<Items...> make_menu(Items &&... items)
{
    return Menu<Items...>(items...);
}
}

#endif
