// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Dump.hxx"
#include "ToJson.hxx"

#include <nlohmann/json.hpp>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace Lua {

int
DumpJson(lua_State *L)
{
	if (lua_gettop(L) < 1)
		return luaL_error(L, "Not enough parameters");

	if (lua_gettop(L) > 1)
		return luaL_error(L, "Too many parameters");

	const auto json = ToJson(L, 1).dump();
	lua_pushlstring(L, json.data(), json.size());
	return 1;
}

} // namespace Lua
