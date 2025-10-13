// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

extern "C" {
#include <lua.h>
}

namespace Lua {

struct LightUserData {
	void *value;

	explicit constexpr LightUserData(void *_value) noexcept
		:value(_value) {}
};

[[gnu::nonnull]]
static inline void
Push(lua_State *L, LightUserData value) noexcept
{
	lua_pushlightuserdata(L, value.value);
}

} // namespace Lua
