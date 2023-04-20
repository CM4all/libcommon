// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef LUA_OBJECT_HXX
#define LUA_OBJECT_HXX

#include "Util.hxx"
#include "Assert.hxx"

extern "C" {
#include <lauxlib.h>
}

#include <memory>
#include <type_traits>

namespace Lua {

/**
 * Helper to wrap a C++ class in a Lua metatable.  This allows
 * instantiating C++ objects managed by Lua.
 */
template<typename T, const char *name>
struct Class {
	using value_type = T;
	using pointer = T *;
	using reference = T &;

	/**
	 * Register the Lua metatable and leave it on the stack.  This
	 * must be called once before New().
	 */
	static void Register(lua_State *L) {
		const ScopeCheckStack check_stack(L, 1);

		luaL_newmetatable(L, name);

		/* let Lua's garbage collector call the destructor
		   (but only if there is one) */
		if constexpr (!std::is_trivially_destructible_v<T>)
			SetField(L, RelativeStackIndex{-1}, "__gc", l_gc);
	}

	/**
	 * Create a new instance, push it on the Lua stack and return
	 * the native pointer.  It will be deleted automatically by
	 * Lua's garbage collector.
	 *
	 * You must call Register() once before calling this method.
	 */
	template<typename... Args>
	static pointer New(lua_State *L, Args&&... args) {
		const ScopeCheckStack check_stack(L, 1);

		T *p = static_cast<T *>(lua_newuserdata(L, sizeof(value_type)));
		luaL_getmetatable(L, name);
		lua_setmetatable(L, -2);

		try {
			return std::construct_at(p, std::forward<Args>(args)...);
		} catch (...) {
			lua_pop(L, 1);
			throw;
		}
	}

	/**
	 * Extract the native pointer from the Lua object on the
	 * stack.  Returns nullptr if the type is wrong.
	 */
	[[gnu::pure]]
	static pointer Check(lua_State *L, int idx) {
		const ScopeCheckStack check_stack(L);

		void *p = lua_touserdata(L, idx);
		if (p == nullptr)
			return nullptr;

		if (lua_getmetatable(L, idx) == 0)
			return nullptr;

		lua_getfield(L, LUA_REGISTRYINDEX, name);
		bool equal = lua_rawequal(L, -1, -2);
		lua_pop(L, 2);
		if (!equal)
			return nullptr;

		return pointer(p);
	}

	/**
	 * Extract the native value from the Lua object on the
	 * stack.  Raise a Lua error if the type is wrong.
	 */
	[[gnu::pure]]
	static reference Cast(lua_State *L, int idx) {
		return *(pointer)luaL_checkudata(L, idx, name);
	}

private:
	static int l_gc(lua_State *L) {
		const ScopeCheckStack check_stack(L);

		auto *p = Check(L, 1);
		/* call the destructor when this instance is
		   garbage-collected */
		std::destroy_at(p);
		return 0;
	}
};

}

#endif
