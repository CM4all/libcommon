// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "Assert.hxx"

extern "C" {
#include <lua.h>
}

#include <utility>
#include <tuple>

namespace Lua {

template<typename... T>
struct CClosure {
	lua_CFunction fn;

	std::tuple<T...> values;
};

template<typename... T>
CClosure<T...>
MakeCClosure(lua_CFunction fn, T&&... values)
{
	return {fn, std::make_tuple(std::forward<T>(values)...)};
}

template<typename... T>
[[gnu::nonnull]]
void
PushAll(lua_State *L, const std::tuple<T...> &t)
{
	const ScopeCheckStack check_stack(L, sizeof...(T));

	std::apply([L](const T&... args){
		(Push(L, args), ...);
	}, t);
}

template<typename... T>
[[gnu::nonnull]]
void
Push(lua_State *L, const CClosure<T...> &value) noexcept
{
	PushAll(L, value.values);
	lua_pushcclosure(L, value.fn, sizeof...(T));
}

} // namespace Lua
