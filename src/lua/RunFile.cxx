// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "RunFile.hxx"
#include "Error.hxx"

extern "C" {
#include <lauxlib.h>
}

void
Lua::RunFile(lua_State *L, const char *path)
{
	if (luaL_loadfile(L, path) || lua_pcall(L, 0, 0, 0))
		throw PopError(L);
}
