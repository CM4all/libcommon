// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

extern "C" {
#include <lua.h>
}

#include <chrono>

namespace std::chrono {

/**
 * Push an arbitrary duration to the Lua stack as a number of seconds.
 * This may require rounding or casting to floating point.
 */
template<class Rep, class Period>
static inline void
Push(lua_State *L, duration<Rep, Period> value) noexcept
{
	if constexpr (std::is_integral_v<Rep> && Period::den == 1)
		/* if it's an integer and the denominator is 1, it can
		   be represented as a lua_Integer */
		// TODO what about overflow?
		lua_pushinteger(L, duration_cast<duration<lua_Integer>>(value).count());
	else
		/* everything else gets casted to floating point */
		lua_pushnumber(L, duration_cast<duration<lua_Number>>(value).count());
}

/**
 * Push the time point to the Lua stack in the usual Lua time stamp
 * format (i.e. seconds since epoch).
 */
static inline void
Push(lua_State *L, system_clock::time_point value) noexcept
{
	// TODO support fractional seconds?
	lua_pushinteger(L, static_cast<lua_Integer>(system_clock::to_time_t(value)));
}

} // namespace std::chrono

namespace Lua {

static inline std::chrono::duration<lua_Number>
ToDuration(lua_State *L, int idx)
{
	return std::chrono::duration<lua_Number>(lua_tonumber(L, idx));
}

static inline std::chrono::system_clock::time_point
ToSystemTimePoint(lua_State *L, int idx)
{
	// TODO support fractional seconds?
	return std::chrono::system_clock::from_time_t(lua_tointeger(L, static_cast<std::time_t>(idx)));
}

} // namespace Lua
