// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Init.hxx"
#include "Dump.hxx"
#include "lua/Util.hxx"

extern "C" {
#include <lua.h>
}

namespace Lua {

void
InitJson(lua_State *L) noexcept
{
	lua_newtable(L);
	SetTable(L, RelativeStackIndex{-1}, "dump", DumpJson);
	lua_setglobal(L, "json");
}

} // namespace Lua
