// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Assert.hxx"
#include "Close.hxx"
#include "ForEach.hxx"
#include "Table.hxx"
#include "Util.hxx"

#include <cstddef>

namespace Lua {

/**
 * A unique pointer to be used as LUA_REGISTRYINDEX key.
 */
inline std::byte auto_close_table;

inline void
PushAutoCloseTable(lua_State *L)
{
	const ScopeCheckStack check_stack{L, 1};

	GetTable(L, LUA_REGISTRYINDEX, LightUserData{&auto_close_table});
}

inline void
PushOrMakeAutoCloseTable(lua_State *L)
{
	const ScopeCheckStack check_stack{L, 1};

	PushAutoCloseTable(L);

	if (lua_isnil(L, -1)) {
		lua_pop(L, 1);

		NewWeakKeyTable(L);
		SetTable(L, LUA_REGISTRYINDEX,
			 LightUserData{&auto_close_table},
			 RelativeStackIndex{-1});
	}
}

/**
 * Schedule a value to be auto-closed by Close().
 *
 * @param key the registry key where the object will be registered
 * @param value the object to be closed; only a weak reference will be
 * stored
 */
template<typename K, typename V>
inline void
AddAutoClose(lua_State *L, K &&key, V &&value)
{
	const ScopeCheckStack check_stack{L};

	// Push(Registry[auto_close_table])
	PushOrMakeAutoCloseTable(L);
	StackPushed(key);
	StackPushed(value);

	// Push(auto_close_table[key])
	GetTable(L, RelativeStackIndex{-1}, key);
	if (lua_isnil(L, -1)) {
		lua_pop(L, 1);
		NewWeakKeyTable(L);
		SetTable(L, RelativeStackIndex{-2}, key,
			 RelativeStackIndex{-1});
	}

	StackPushed(value);

	// auto_close_table[key][value] = 1
	SetTable(L, RelativeStackIndex{-1}, std::forward<V>(value), lua_Integer{1});

	// pop auto_close_table[key], auto_close_table
	lua_pop(L, 2);
}

/**
 * Close all registered objects for the given key.
 */
template<typename K>
inline void
AutoClose(lua_State *L, K &&key)
{
	const ScopeCheckStack check_stack{L};

	PushAutoCloseTable(L);
	if (lua_isnil(L, -1)) {
		lua_pop(L, 1);
		return;
	}

	StackPushed(key);

	GetTable(L, RelativeStackIndex{-1}, std::forward<K>(key));
	if (lua_isnil(L, -1)) {
		lua_pop(L, 2);
		return;
	}

	ForEach(L, RelativeStackIndex{-1}, [L](auto key_idx, auto){
		Close(L, key_idx);
	});

	lua_pop(L, 2);
}

} // namespace Lua
