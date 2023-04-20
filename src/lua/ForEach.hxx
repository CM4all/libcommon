// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Assert.hxx"
#include "StackIndex.hxx"

#include <concepts>

#ifdef LUA_LJDIR
#include <exception>
#endif

struct lua_State;

namespace Lua {

/**
 * Call the given lambda for each entry in the table.
 */
inline void
ForEach(lua_State *L, auto table_idx, auto f)
{
	const ScopeCheckStack check_stack{L};

	lua_pushnil(L);
	StackPushed(table_idx);

	while (lua_next(L, GetStackIndex(table_idx))) {
		try {
			f(RelativeStackIndex{-2}, RelativeStackIndex{-1});
		} catch (...) {
			/* pop key and value */

#ifdef LUA_LJDIR
			if (std::current_exception()) {
#endif
				/* this is a C++ exception */
				lua_pop(L, 2);
#ifdef LUA_LJDIR
			} else {
				/* this is a lua_error() (only
				   supported on LuaJit) and the error
				   is on the top of the Lua stack; we
				   need to use lua_remove() to remove
				   key and value behind it */
				lua_remove(L, -2);
				lua_remove(L, -2);
			}
#endif

			throw;
		}

		/* pop the value */
		lua_pop(L, 1);
	}

	/* the last lua_next() pops both */
}

} // namespace Lua
