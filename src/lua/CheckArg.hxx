// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include <cstddef>
#include <span>
#include <string_view>

namespace Lua {

inline std::string_view
CheckStringView(lua_State *L, int arg)
{
	std::size_t size;
	const char *data = luaL_checklstring(L, arg, &size);
	return {data, size};
}

inline std::span<const std::byte>
CheckByteSpan(lua_State *L, int arg)
{
	std::size_t size;
	const char *data = luaL_checklstring(L, arg, &size);
	return {reinterpret_cast<const std::byte *>(data), size};
}

} // namespace Lua
