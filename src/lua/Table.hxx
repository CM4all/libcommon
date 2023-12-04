// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Assert.hxx"
#include "Util.hxx"

namespace Lua {

inline void
NewTableWithMode(lua_State *L, const char *mode)
{
	const ScopeCheckStack check_stack{L, 1};

	lua_newtable(L);
	lua_newtable(L);
	SetTable(L, RelativeStackIndex{-1}, "__mode", mode);
	lua_setmetatable(L, -2);
}

inline void
NewWeakKeyTable(lua_State *L)
{
	NewTableWithMode(L, "k");
}

} // namespace Lua
