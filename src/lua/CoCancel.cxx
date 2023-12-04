// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "CoCancel.hxx"
#include "Assert.hxx"
#include "Close.hxx"

extern "C" {
#include <lua.h>
}

namespace Lua {

static bool
CoCancel(lua_State *L, int idx)
{
	return Close(L, idx);
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
