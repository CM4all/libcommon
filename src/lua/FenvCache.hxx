// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "Assert.hxx"
#include "StackIndex.hxx"
#include "Util.hxx"

namespace Lua {

/**
 * Look up an item in a userdata's fenv table.
 *
 * @return true if the item was found (value is left on the Lua
 * stack), false if the item was not found (Lua stack is unmodified)
 */
template<typename K>
inline bool
GetFenvCache(lua_State *L, AnyStackIndex auto userdata_idx, K &&key)
{
	ScopeCheckStack check_stack{L};

	lua_getfenv(L, StackIndex{userdata_idx}.idx);
	StackPushed(key);

	GetTable(L, RelativeStackIndex{-1}, key);

	if (lua_isnil(L, -1)) {
		lua_pop(L, 2);
		return false;
	} else {
		// remove the fenv from the Lua stack
		lua_remove(L, -2);

		++check_stack;
		return true;
	}
}

/**
 * Look up an item in a userdata's fenv table.
 *
 * @return true if the item was found (value is left on the Lua
 * stack), false if the item was not found (Lua stack is unmodified)
 */
template<typename K, typename V>
inline void
SetFenvCache(lua_State *L, AnyStackIndex auto userdata_idx,
	     K &&key, V &&value)
{
	const ScopeCheckStack check_stack{L};

	lua_getfenv(L, StackIndex{userdata_idx}.idx);
	StackPushed(key);
	StackPushed(value);

	SetTable(L, RelativeStackIndex{-1},
		 std::forward<K>(key),
		 std::forward<V>(value));

	lua_pop(L, 1);
}

}
