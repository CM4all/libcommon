// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "ScalarMult.hxx"
#include "lua/CheckArg.hxx"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include <sodium/crypto_scalarmult.h>

namespace Lua {

int
crypto_scalarmult_base(lua_State *L)
{
	if (lua_gettop(L) > 1)
		return luaL_error(L, "Too many parameters");

	const auto sk = CheckStringView(L, 1);
	luaL_argcheck(L, sk.size() == crypto_scalarmult_SCALARBYTES, 1,
		      "Malformed secret key");

	unsigned char pk[crypto_scalarmult_BYTES];
	::crypto_scalarmult_base(pk,
				 reinterpret_cast<const unsigned char *>(sk.data()));

	lua_pushlstring(L, reinterpret_cast<const char *>(pk),
			sizeof(pk));
	return 1;
}

} // namespace Lua
