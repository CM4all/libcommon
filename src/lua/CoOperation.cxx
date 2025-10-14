// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "CoOperation.hxx"
#include "Assert.hxx"
#include "Close.hxx"

extern "C" {
#include <lua.h>
}

#include <cassert>

namespace Lua {

int
YieldOperation(lua_State *L) noexcept
{
	assert(lua_gettop(L) >= 1);
	assert(lua_isuserdata(L, -1));

	return lua_yield(L, 1);
}

void
PushOperation(lua_State *L)
{
	assert(lua_status(L) == LUA_YIELD);

	/* the current implementation assumes the operation object is
	   already on the stack */
	assert(lua_gettop(L) == 1);
	assert(lua_isuserdata(L, 1));

	// TODO implement
	lua_pushvalue(L, -1);
}

void
ConsumeOperation(lua_State *L)
{
	assert(lua_status(L) == LUA_YIELD);

	/* the current implementation assumes the operation object is
	   already on the stack */
	assert(lua_gettop(L) == 1);
	assert(lua_isuserdata(L, 1));

	// TODO implement
	(void)L;
}

static bool
CancelOperation(lua_State *L, AnyStackIndex auto idx)
{
	return Close(L, idx);
}

bool
CancelOperation(lua_State *L)
{
	const ScopeCheckStack check_stack{L};

	if (lua_status(L) != LUA_YIELD)
		/* not suspended by a blocking operation via
		   lua_yield() */
		return false;

	if (lua_gettop(L) != 1 || !lua_isuserdata(L, 1))
		/* not a "userdata" object on the stack */
		return false;

	return CancelOperation(L, 1);
}

} // namespace Lua
