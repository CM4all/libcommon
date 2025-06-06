// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

extern "C" {
#include <lua.h>
}

#include <string_view>

namespace Lua {

[[gnu::pure]]
inline std::string_view
ToStringView(lua_State *L, int idx) noexcept
{
	size_t length;
	const char *s = lua_tolstring(L, idx, &length);
	return {s, length};
}

} // namespace Lua
