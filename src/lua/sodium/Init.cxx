// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Init.hxx"
#include "Box.hxx"
#include "RandomBytes.hxx"
#include "ScalarMult.hxx"
#include "Utils.hxx"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include <sodium/core.h>

#include <stdexcept>

namespace Lua {

static constexpr struct luaL_Reg lua_sodium[] = {
	{"crypto_box_keypair", crypto_box_keypair},
	{"crypto_box_seal", crypto_box_seal},
	{"crypto_box_seal_open", crypto_box_seal_open},
	{"crypto_scalarmult_base", crypto_scalarmult_base},
	{"bin2hex", bin2hex},
	{"hex2bin", hex2bin},
	{"randombytes", randombytes},
	{nullptr, nullptr}
};

void
InitSodium(lua_State *L)
{
	if (sodium_init() == -1)
		throw std::runtime_error{"sodium_init() failed"};

	luaL_newlib(L, lua_sodium);
	lua_setglobal(L, "sodium");
}

} // namespace Lua
