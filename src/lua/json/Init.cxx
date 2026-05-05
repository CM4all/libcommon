// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Init.hxx"
#include "Dump.hxx"

extern "C" {
#include <lua.h>
}

namespace Lua {

void
InitJson(lua_State *L) noexcept
{
	lua_pushcfunction(L, DumpJson);
	lua_setglobal(L, "to_json");
}

} // namespace Lua
