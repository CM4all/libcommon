// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Assert.hxx"
#include "StackIndex.hxx"

#include <concepts>

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
			lua_pop(L, 2);
			throw;
		}

		/* pop the value */
		lua_pop(L, 1);
	}

	/* the last lua_next() pops both */
}

} // namespace Lua
