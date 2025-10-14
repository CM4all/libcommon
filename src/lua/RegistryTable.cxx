// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "RegistryTable.hxx"
#include "Assert.hxx"
#include "Util.hxx"
#include "LightUserData.hxx"

extern "C" {
#include <lua.h>
}

#include <cassert>

namespace Lua {

bool
GetRegistryTable(lua_State *L, LightUserData key)
{
	ScopeCheckStack check_stack{L};

	GetTable(L, LUA_REGISTRYINDEX, key);

	const bool result = !lua_isnil(L, -1);
	if (result) [[likely]] {
		assert(lua_istable(L, -1));

		++check_stack;
	} else {
		/* pop nil */
		lua_pop(L, 1);
	}

	return result;
}

void
MakeRegistryTable(lua_State *L, LightUserData key)
{
	const ScopeCheckStack check_stack{L, 1};

	if (!GetRegistryTable(L, key)) {
		/* create a new table */
		lua_newtable(L);

		/* registry[key] = newtable */
		SetTable(L, LUA_REGISTRYINDEX, key,
			 RelativeStackIndex{-1});
	}
}

} // namespace Lua
