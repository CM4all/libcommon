// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "RandomBytes.hxx"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include <sodium/randombytes.h>

#include <memory>

namespace Lua {

int
randombytes(lua_State *L)
{
	if (lua_gettop(L) > 1)
		return luaL_error(L, "Too many parameters");

	const auto size = luaL_checkinteger(L, 1);
	luaL_argcheck(L, size >= 1, 1, "Size is too small");
	luaL_argcheck(L, size <= 1024 * 1024, 1, "Size is too large");

	auto dest = std::make_unique<char[]>(size);
	randombytes_buf(dest.get(), size);

	lua_pushlstring(L, dest.get(), size);
	return 1;
}

} // namespace Lua
