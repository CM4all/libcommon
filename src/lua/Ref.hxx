// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Util.hxx"

#include <utility>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace Lua {

struct Pop {};

/**
 * A reference to a Lua object.
 */
class Ref {
	lua_State *L;

	int ref = LUA_NOREF;

public:
	Ref() noexcept = default;

	/**
	 * Convert the top of the stack into a reference (and pop it
	 * from the stack).
	 */
	Ref(lua_State *_L, Pop) noexcept
		:L(_L), ref(luaL_ref(L, LUA_REGISTRYINDEX)) {}

	template<typename V>
	Ref(lua_State *_L, V &&value)
		:L(_L)
	{
		Lua::Push(_L, std::forward<V>(value));
		ref = luaL_ref(L, LUA_REGISTRYINDEX);
	}

	Ref(Ref &&src) noexcept
		:L(src.L), ref(std::exchange(src.ref, LUA_NOREF)) {}

	~Ref() noexcept {
		if (ref != LUA_NOREF)
			luaL_unref(L, LUA_REGISTRYINDEX, ref);
	}

	Ref &operator=(Ref &&src) noexcept {
		using std::swap;
		swap(L, src.L);
		swap(ref, src.ref);
		return *this;
	}

	operator bool() const noexcept {
		return ref != LUA_NOREF;
	}

	/**
	 * Push the value on the given thread's stack.
	 */
	void Push(lua_State *thread_L) const {
		lua_rawgeti(thread_L, LUA_REGISTRYINDEX, ref);
	}
};

static inline void
Push(lua_State *L, const Ref &ref)
{
	ref.Push(L);
}

} // namespace Lua
