// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Box.hxx"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include <sodium/crypto_box.h>

#include <memory>
#include <string_view>

namespace Lua {

static std::string_view
CheckString(lua_State *L, int arg)
{
	std::size_t size;
	const char *data = luaL_checklstring(L, arg, &size);
	return {data, size};
}

int
crypto_box_keypair(lua_State *L)
{
	if (lua_gettop(L) > 0)
		return luaL_error(L, "Too many parameters");

	unsigned char pk[crypto_box_PUBLICKEYBYTES];
	unsigned char sk[crypto_box_SECRETKEYBYTES];
	::crypto_box_keypair(pk, sk);

	lua_pushlstring(L, reinterpret_cast<const char *>(pk),
			crypto_box_PUBLICKEYBYTES);
	lua_pushlstring(L, reinterpret_cast<const char *>(sk),
			crypto_box_SECRETKEYBYTES);
	return 2;
}

int
crypto_box_seal(lua_State *L)
{
	if (lua_gettop(L) > 2)
		return luaL_error(L, "Too many parameters");

	const auto m = CheckString(L, 1);

	const auto pk = CheckString(L, 2);
	luaL_argcheck(L, pk.size() == crypto_box_PUBLICKEYBYTES, 2,
		      "Malformed public key");

	std::unique_ptr<char[]> c(new char[m.size() + crypto_box_SEALBYTES]);

	if (::crypto_box_seal(reinterpret_cast<unsigned char *>(c.get()),
			      reinterpret_cast<const unsigned char *>(m.data()),
			      m.size(),
			      reinterpret_cast<const unsigned char *>(pk.data()))  != 0)
		return 0;

	lua_pushlstring(L, c.get(), m.size() + crypto_box_SEALBYTES);
	return 1;
}

int
crypto_box_seal_open(lua_State *L)
{
	if (lua_gettop(L) > 3)
		return luaL_error(L, "Too many parameters");

	const auto c = CheckString(L, 1);
	luaL_argcheck(L, c.size() >= crypto_box_SEALBYTES, 1,
		      "Malformed ciphertext");

	const auto pk = CheckString(L, 2);
	luaL_argcheck(L, pk.size() == crypto_box_PUBLICKEYBYTES, 2,
		      "Malformed public key");

	const auto sk = CheckString(L, 3);
	luaL_argcheck(L, sk.size() == crypto_box_SECRETKEYBYTES, 3,
		      "Malformed secret key");

	std::unique_ptr<char[]> m(new char[c.size() - crypto_box_SEALBYTES]);

	if (::crypto_box_seal_open(reinterpret_cast<unsigned char *>(m.get()),
				   reinterpret_cast<const unsigned char *>(c.data()),
				   c.size(),
				   reinterpret_cast<const unsigned char *>(pk.data()),
				   reinterpret_cast<const unsigned char *>(sk.data())) != 0)
		return 0;

	lua_pushlstring(L, m.get(), c.size() - crypto_box_SEALBYTES);
	return 1;
}

} // namespace Lua
