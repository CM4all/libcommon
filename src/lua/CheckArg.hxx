// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include <string_view>

namespace Lua {

inline std::string_view
CheckStringView(lua_State *L, int arg)
{
	std::size_t size;
	const char *data = luaL_checklstring(L, arg, &size);
	return {data, size};
}

} // namespace Lua
