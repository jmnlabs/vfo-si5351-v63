/**
 * @file    utilities.h
 * @brief   Minimal freestanding template utilities for AVR.
 *
 * Self-contained replacements for parts of the C++ standard library that are
 * awkward on AVR: a fixed-size Array with constexpr make_array(), a compile-
 * time integer sequence, a small enable_if, and a lightweight Tuple with both
 * compile-time and runtime (visit_ith) element visitation used by the menu.
 *
 * @author  Jarosław Marek Niewiński (current version and modifications)
 * @author  Documentation and implementation assistance: Claude (Anthropic AI)
 * @note    Based on the VFO project by Jan Ciger (Janoc):
 *          https://janoc.rd-h.com/archives/649
 * @license GNU General Public License v3.0 (GPLv3)
 */

#pragma once

namespace Utilities
{

///////////////////////////////////////////////////////////////////////////////
//                                   Array                                   //
///////////////////////////////////////////////////////////////////////////////
template <typename T, unsigned N> struct Array
{
    typedef T value_type;
    typedef T &reference;
    typedef const T &const_reference;
    typedef value_type* iterator;
    typedef const value_type* const_iterator;
    typedef unsigned size_type;

    // public member, no constructors = will use aggregate initialization
    T data[N];

    iterator begin() noexcept { return iterator(&data[0]); }
    const_iterator begin() const noexcept { return const_iterator(&data[0]); }
    iterator end() noexcept { return iterator(&data[0] + N); }
    const_iterator end() const noexcept { return const_iterator(&data[0] + N); }

    reference operator[](size_type n) noexcept { return data[n]; }
    constexpr const_reference operator[](size_type n) const noexcept { return data[n]; }

    constexpr size_type size() const noexcept { return N; }
    constexpr bool empty() const noexcept { return size() == 0; }
};

template<typename Arg, typename ...Args>
constexpr auto make_array(Arg a, Args... args) -> Array<decltype(a), sizeof...(args) + 1>
{
    return Array<Arg, sizeof...(args) + 1> {{a, args...}};
}

///////////////////////////////////////////////////////////////////////////////
//                              Integer sequence                             //
///////////////////////////////////////////////////////////////////////////////

template <int... Ns> struct integer_seq
{
    static constexpr int size = sizeof...(Ns);
};

namespace detail
{

template <int I, int... Ns> struct integer_seq_gen;

template <int I, int... Ns> struct integer_seq_gen
{
    using type = typename integer_seq_gen<I - 1, I - 1, Ns...>::type;
};

template <int... Ns> struct integer_seq_gen<0, Ns...>
{
    using type = integer_seq<Ns...>;
};
}

template <int N> using integer_seq_t = typename detail::integer_seq_gen<N>::type;

///////////////////////////////////////////////////////////////////////////
//                               enable_if                               //
///////////////////////////////////////////////////////////////////////////

template <bool B, typename T = void> struct enable_if
{
};

template <typename T> struct enable_if<true, T>
{
    using type = T;
};

///////////////////////////////////////////////////////////////////////////////
//                             Simple Tuple class                            //
///////////////////////////////////////////////////////////////////////////////

namespace detail
{

template <unsigned I, class T> struct TupleElement
{
    T val;
};

template <unsigned I, typename... Ts> struct TupleImpl
{
};

// empty tuple
template <unsigned I>
struct TupleImpl<I>
{};

template <unsigned I, typename E, typename... Rest>
struct TupleImpl<I, E, Rest...> : TupleElement<I, E> , TupleImpl<I + 1, Rest...>
{
    explicit constexpr TupleImpl(E e, Rest... rest)
        : TupleElement<I, E>{e} , TupleImpl<I + 1, Rest...>(rest...) {}
};
};

template <typename... Ts>
struct Tuple : detail::TupleImpl<0, Ts...>
{
    explicit constexpr Tuple(Ts... ts) : detail::TupleImpl<0, Ts...>(ts...) {}
    template <unsigned Idx, class E> static E &get_i(detail::TupleElement<Idx, E> &e) { return e.val; }
    static constexpr unsigned size = sizeof...(Ts);
};

template <unsigned I, typename... Ts> auto get(Tuple<Ts...> &t) -> decltype(t.template get_i<I>(t))
{
    return t.template get_i<I>(t);
}

template <typename... Ts>
constexpr Tuple<Ts...> make_tuple(Ts... es)
{
    return Tuple<Ts...>{es...};
}

// Tuple Visitor //////////////////////////////////////////////////////////////
namespace detail
{
template <unsigned idx, typename Visitor, typename Tuple> struct VisitTupleImpl
{
    static void process(Visitor &v, Tuple &t)
    {
        VisitTupleImpl<idx - 1, Visitor, Tuple>::process(v, t);
        v(get<idx>(t));
    }
};


template <typename Visitor, typename Tuple> struct VisitTupleImpl<0, Visitor, Tuple>
{
    static void process(Visitor &v, Tuple &t) { v(get<0>(t)); }
};
}

template <typename Visitor, typename First, typename... Rest>
void visit_tuple(Visitor v, Tuple<First, Rest...> &t)
{
    detail::VisitTupleImpl<sizeof...(Rest), Visitor, Tuple<First, Rest...>>::process(v, t);
}

// Tuple visitor with runtime indexing ////////////////////////////////////////
namespace detail
{
template <typename Visitor, typename Tuple>
void visit_ith_impl(int i, Visitor &v, Tuple &t, Utilities::integer_seq<>)
{
    if (i == 0)
        v(get<0>(t));
}

template <typename Visitor, typename Tuple, int I, int... Ns>
void visit_ith_impl(int i, Visitor &v, Tuple &t, Utilities::integer_seq<I, Ns...>)
{
    if (i == I)
        v(get<I>(t));
    else
        visit_ith_impl(i, v, t, Utilities::integer_seq<Ns...>{});
}
}

template <typename Visitor, typename Tuple> void visit_ith(int i, Visitor &&v, Tuple &t)
{
    detail::visit_ith_impl(i, v, t, Utilities::integer_seq_t<Tuple::size>{});
}
}
