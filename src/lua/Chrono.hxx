// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

extern "C" {
#include <lua.h>
}

#include <chrono>

namespace std::chrono {

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

static inline std::chrono::system_clock::time_point
ToSystemTimePoint(lua_State *L, int idx)
{
	// TODO support fractional seconds?
	return std::chrono::system_clock::from_time_t(lua_tointeger(L, static_cast<std::time_t>(idx)));
}

} // namespace Lua
