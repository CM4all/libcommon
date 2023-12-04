// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "CoCancel.hxx"
#include "Assert.hxx"

extern "C" {
#include <lua.h>
}

namespace Lua {

static bool
CoCancel(lua_State *L, int idx)
{
	const ScopeCheckStack check_main_stack{L};

	lua_getmetatable(L, idx);
	if (!lua_istable(L, -1)) {
		/* pop nil */
		lua_pop(L, 1);
		return false;
	}

	lua_getfield(L, -1, "__close");
	if (!lua_isfunction(L, -1)) {
		/* pop nil and metatable */
		lua_pop(L, 2);
		return false;
	}

	/* call __close(self), ignore return values and errors */
	lua_pushvalue(L, idx);
	lua_pcall(L, 1, 0, 0);

	/* pop metatable */
	lua_pop(L, 1);

	return true;
}

bool
CoCancel(lua_State *L)
{
	const ScopeCheckStack check_main_stack{L};

	if (lua_status(L) != LUA_YIELD)
		/* not suspended by a blocking operation via
		   lua_yield() */
		return false;

	if (lua_gettop(L) != 1 || !lua_isuserdata(L, 1))
		/* not a "userdata" object on the stack */
		return false;

	return CoCancel(L, 1);
}

} // namespace Lua
