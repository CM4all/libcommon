/*
 * Copyright 2021 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
