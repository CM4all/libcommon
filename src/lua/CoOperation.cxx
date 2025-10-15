// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "CoOperation.hxx"
#include "Assert.hxx"
#include "Close.hxx"
#include "LightUserData.hxx"
#include "RegistryTable.hxx"
#include "Util.hxx"

extern "C" {
#include <lua.h>
}

#include <cassert>

namespace Lua {

/**
 * A global variable used to build a unique LightUserData for storing
 * the operation that has yielded.
 */
static int current_operation_id;

int
YieldOperation(lua_State *L) noexcept
{
	assert(lua_gettop(L) >= 1);
	assert(lua_isuserdata(L, -1));

	RelativeStackIndex idx{-1};

	MakeRegistryTable(L, LightUserData{&current_operation_id});
	StackPushed(idx);

	SetTable(L, RelativeStackIndex{-1}, CurrentThread{}, idx);

	/* operation and the table are still on the stack, but don't
	   bother to pop them; the lua_yield() will clear the stack
	   anyway */

	return lua_yield(L, 0);
}

void
PushOperation(lua_State *L)
{
	assert(lua_status(L) == LUA_YIELD);

	const ScopeCheckStack check_stack{L, 1};

	if (!GetRegistryTable(L, LightUserData{&current_operation_id})) {
		assert(false);
	}

	GetTable(L, RelativeStackIndex{-1}, CurrentThread{});

	/* remove table from Lua stack */
	lua_remove(L, -2);
}

/**
 * A variant of ConsumeOperation() with runtime checks instead of
 * assertions.  Use this when you don't know whether there's a yielded
 * operation.
 */
static bool
CheckConsumeOperation(lua_State *L)
{
	ScopeCheckStack check_stack{L};

	if (lua_status(L) != LUA_YIELD)
		/* not suspended by a blocking operation via
		   lua_yield() */
		return false;

	if (!GetRegistryTable(L, LightUserData{&current_operation_id}))
		/* without the table, no ongoing operation can
		   possibly be registered */
		return false;

	GetTable(L, RelativeStackIndex{-1}, CurrentThread{});
	if (lua_isnil(L, -1)) {
		/* no operation in the table */

		/* pop nil and table */
		lua_pop(L, 2);
		return false;
	}

	SetTable(L, RelativeStackIndex{-2}, CurrentThread{}, nullptr);

	/* pop table */
	lua_remove(L, -2);

	++check_stack;
	return true;
}

void
ConsumeOperation(lua_State *L)
{
	assert(lua_status(L) == LUA_YIELD);

	const ScopeCheckStack check_stack{L, 1};

	if (!GetRegistryTable(L, LightUserData{&current_operation_id})) {
		assert(false);
	}

	GetTable(L, RelativeStackIndex{-1}, CurrentThread{});
	SetTable(L, RelativeStackIndex{-2}, CurrentThread{}, nullptr);

	/* remove table from Lua stack */
	lua_remove(L, -2);
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

	if (!CheckConsumeOperation(L))
		return false;

	bool result = CancelOperation(L, RelativeStackIndex{-1});

	/* pop the operation object */
	lua_pop(L, 1);

	return result;
}

} // namespace Lua
