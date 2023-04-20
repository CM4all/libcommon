// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Init.hxx"
#include "Box.hxx"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace Lua {

static constexpr struct luaL_Reg lua_sodium[] = {
	{"crypto_box_keypair", crypto_box_keypair},
	{"crypto_box_seal", crypto_box_seal},
	{"crypto_box_seal_open", crypto_box_seal_open},
	{nullptr, nullptr}
};

void
InitSodium(lua_State *L)
{
	luaL_newlib(L, lua_sodium);
	lua_setglobal(L, "sodium");
}

} // namespace Lua
