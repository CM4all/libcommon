// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "State.hxx"

extern "C" {
#include <lua.h>
}

void
Lua::StateDeleter::operator()(lua_State *state) const noexcept
{
	lua_close(state);
}
