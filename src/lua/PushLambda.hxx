// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "Assert.hxx"

extern "C" {
#include <lua.h>
}

#include <utility>

namespace Lua {

/**
 * Internal helper type generated by Lambda().  It is designed to be
 * optimized away.
 */
template<typename T>
struct _Lambda : T {
	template<typename U>
	_Lambda(U &&u):T(std::forward<U>(u)) {}
};

/**
 * Instantiate a #_Lambda instance for the according Push() overload.
 */
template<typename T>
static inline _Lambda<T>
Lambda(T &&t)
{
	return _Lambda<T>(std::forward<T>(t));
}

/**
 * Push a value on the Lua stack by invoking a C++ lambda.  With
 * C++17, we could use std::is_callable, but with C++14, we need the
 * detour with struct _Lambda and Lambda().
 */
template<typename T>
[[gnu::nonnull]]
static inline void
Push(lua_State *L, _Lambda<T> l)
{
	ScopeCheckStack check_stack{L};

	l();

	++check_stack;
}

} // namespace Lua
