// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "StackIndex.hxx"
#include "Assert.hxx"

namespace Lua {

/**
 * Call the __close method.
 *
 * @return true if the __close method was called, false if the object
 * does not have one
 */
inline bool
Close(lua_State *L, AnyStackIndex auto idx)
{
	const ScopeCheckStack check_stack{L};

	lua_getmetatable(L, GetStackIndex(idx));
	if (!lua_istable(L, -1)) {
		/* pop nil */
		lua_pop(L, 1);
		return false;
	}

	lua_getfield(L, -1, "__close");
	if (!lua_isfunction(L, -1)) {
		/* pop nil and metatable */
		lua_pop(L, 2);
		return false;
	}

	StackPushed(idx, 2);

	/* call __close(obj), ignore return values and errors */
	lua_pushvalue(L, GetStackIndex(idx));
	lua_pcall(L, 1, 0, 0);

	/* pop metatable */
	lua_pop(L, 1);

	return true;
}

} // namespace Lua
