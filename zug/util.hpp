//
// zug: transducers for C++
// Copyright (C) 2019 Juan Pedro Bolivar Puente
//
// This software is distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://boost.org/LICENSE_1_0.txt
//

#pragma once

#include <zug/compat/invoke.hpp>
#include <zug/detail/inline_constexpr.hpp>
#include <zug/meta/dispatch.hpp>

#include <tuple>
#include <type_traits>
#include <utility>

#define ZUG_FWD(x) std::forward<decltype(x)>(x)

namespace zug {

/*!
 * Does nothing.
 */
ZUG_INLINE_CONSTEXPR struct noop_t
{
    template <typename... T>
    void operator()(T&&...) const
    {}
} noop{};

/*!
 * Similar to clojure.core/identity
 */
ZUG_INLINE_CONSTEXPR struct identity_t
{
    template <typename T>
    decltype(auto) operator()(T&& x) const
    {
        return ZUG_FWD(x);
    };
} identity{};

/*!
 * Similar to @a identity, but it never returns a reference
 * to the pased in value.
 */
ZUG_INLINE_CONSTEXPR struct identity__t
{
    template <typename T>
    auto operator()(T&& x) const
    {
        return ZUG_FWD(x);
    };
} identity_{};

namespace detail {

template <std::size_t Index>
struct invoke_composition_impl
{
    template <typename Fns, typename... Args>
    static constexpr decltype(auto) apply(Fns&& fns, Args&&... args)
    {
        return invoke_composition_impl<Index - 1>::apply(
            std::forward<Fns>(fns),
            compat::invoke(std::get<Index>(std::forward<Fns>(fns)),
                           std::forward<Args>(args)...));
    }
};

template <>
struct invoke_composition_impl<0>
{
    template <typename Fns, typename... Args>
    static constexpr decltype(auto) apply(Fns&& fns, Args&&... args)
    {
        return compat::invoke(std::get<0>(std::forward<Fns>(fns)),
                              std::forward<Args>(args)...);
    }
};

template <typename Fns, typename... Args>
constexpr decltype(auto) invoke_composition(Fns&& fns, Args&&... args)
{
    constexpr auto Size = std::tuple_size<std::decay_t<Fns>>::value;
    return invoke_composition_impl<Size - 1>::apply(
        std::forward<Fns>(fns), std::forward<Args>(args)...);
}

template <typename Fn, typename... Fns>
struct composed : std::tuple<Fn, Fns...>
{
    using base_t = std::tuple<Fn, Fns...>;

    template <
        typename TupleFns,
        std::enable_if_t<
            !std::is_same<composed, std::decay_t<TupleFns>>::value>* = nullptr>
    constexpr composed(TupleFns&& fns)
        : base_t{std::forward<TupleFns>(fns)}
    {}

    template <typename... T>
    constexpr decltype(auto) operator()(T&&... xs) &
    {
        return invoke_composition(as_tuple(), std::forward<T>(xs)...);
    }

    template <typename... T>
    constexpr decltype(auto) operator()(T&&... xs) const&
    {
        return invoke_composition(as_tuple(), std::forward<T>(xs)...);
    }

    template <typename... T>
    constexpr decltype(auto) operator()(T&&... xs) &&
    {
        return invoke_composition(std::move(*this).as_tuple(),
                                  std::forward<T>(xs)...);
    }

    base_t& as_tuple() & { return *this; }

    const base_t& as_tuple() const& { return *this; }

    base_t&& as_tuple() && { return std::move(*this); }
};

template <typename T>
constexpr bool is_composed_v = false;

template <typename... Fns>
constexpr bool is_composed_v<composed<Fns...>> = true;

template <typename Composed,
          std::enable_if_t<is_composed_v<std::decay_t<Composed>>>* = nullptr>
decltype(auto) to_function_tuple(Composed&& c, meta::try_t)
{
    return std::forward<Composed>(c).as_tuple();
}

template <typename Fn>
auto to_function_tuple(Fn&& fn, meta::catch_t)
{
    return std::make_tuple(std::forward<Fn>(fn));
}

template <typename T>
decltype(auto) to_function_tuple(T&& t)
{
    return to_function_tuple(std::forward<T>(t), meta::try_t{});
}

template <typename T>
struct make_composed_result;

template <typename... Fns>
struct make_composed_result<std::tuple<Fns...>>
{
    using type = composed<Fns...>;
};

template <typename T>
using make_composed_result_t = typename make_composed_result<T>::type;

template <typename TupleFns>
auto make_composed(TupleFns&& fns)
    -> make_composed_result_t<std::decay_t<TupleFns>>
{
    return {std::forward<TupleFns>(fns)};
}

} // namespace detail

/*!
 * Right-to left function composition. Returns an object *g* that
 * composes all the given functions @f$ f_i @f$, such that
 * @f[
 *                g(x) = f_1(f_2(...f_n(x)))
 * @f]
 *
 * Functions are invoked via standard *INVOKE*, allowing to compose
 * function pointers, member functions, etc.
 */
template <typename F>
auto comp(F&& f) -> detail::composed<std::decay_t<F>>
{
    return {std::forward<F>(f)};
}

template <typename Fn, typename... Fns>
auto comp(Fn&& f, Fns&&... fns)
{
    return detail::make_composed(
        std::tuple_cat(detail::to_function_tuple(std::forward<Fn>(f)),
                       detail::to_function_tuple(std::forward<Fns>(fns))...));
}

namespace detail {

template <typename Lhs,
          typename Rhs,
          std::enable_if_t<is_composed_v<std::decay_t<Lhs>> ||
                           is_composed_v<std::decay_t<Rhs>>>* = nullptr>
constexpr auto operator|(Lhs&& lhs, Rhs&& rhs)
{
    return comp(std::forward<Lhs>(lhs), std::forward<Rhs>(rhs));
}

} // namespace detail

/*!
 * @see constantly
 */
template <typename T>
struct constantly_t
{
    T value;

    template <typename... ArgTs>
    auto operator()(ArgTs&&...) & -> T&
    {
        return value;
    }

    template <typename... ArgTs>
    auto operator()(ArgTs&&...) const& -> const T&
    {
        return value;
    }

    template <typename... ArgTs>
    auto operator()(ArgTs&&...) && -> T&&
    {
        return std::move(value);
    }
};

/*!
 * Similar to clojure.core/constantly
 */
template <typename T>
auto constantly(T&& value) -> constantly_t<std::decay_t<T>>
{
    return constantly_t<std::decay_t<T>>{std::forward<T>(value)};
}

} // namespace zug
