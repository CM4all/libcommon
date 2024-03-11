// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Box.hxx"
#include "lua/CheckArg.hxx"
#include "lib/sodium/Box.hxx"
#include "util/SpanCast.hxx"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include <memory>

namespace Lua {

int
crypto_box_keypair(lua_State *L)
{
	if (lua_gettop(L) > 0)
		return luaL_error(L, "Too many parameters");

	CryptoBoxPublicKey pk;
	CryptoBoxSecretKey sk;
	::crypto_box_keypair(pk, sk);

	lua_pushlstring(L, reinterpret_cast<const char *>(pk.data()), pk.size());
	lua_pushlstring(L, reinterpret_cast<const char *>(sk.data()), sk.size());
	return 2;
}

int
crypto_box_seal(lua_State *L)
{
	if (lua_gettop(L) > 2)
		return luaL_error(L, "Too many parameters");

	const auto m = CheckStringView(L, 1);

	const auto pk = CheckStringView(L, 2);
	luaL_argcheck(L, pk.size() == crypto_box_PUBLICKEYBYTES, 2,
		      "Malformed public key");

	std::unique_ptr<char[]> c(new char[m.size() + crypto_box_SEALBYTES]);

	::crypto_box_seal(reinterpret_cast<std::byte *>(c.get()),
			  AsBytes(m),
			  AsBytes(pk).first<crypto_box_PUBLICKEYBYTES>());

	lua_pushlstring(L, c.get(), m.size() + crypto_box_SEALBYTES);
	return 1;
}

int
crypto_box_seal_open(lua_State *L)
{
	if (lua_gettop(L) > 3)
		return luaL_error(L, "Too many parameters");

	const auto c = CheckStringView(L, 1);
	luaL_argcheck(L, c.size() >= crypto_box_SEALBYTES, 1,
		      "Malformed ciphertext");

	const auto pk = CheckStringView(L, 2);
	luaL_argcheck(L, pk.size() == crypto_box_PUBLICKEYBYTES, 2,
		      "Malformed public key");

	const auto sk = CheckStringView(L, 3);
	luaL_argcheck(L, sk.size() == crypto_box_SECRETKEYBYTES, 3,
		      "Malformed secret key");

	std::unique_ptr<char[]> m(new char[c.size() - crypto_box_SEALBYTES]);

	if (::crypto_box_seal_open(reinterpret_cast<std::byte *>(m.get()),
				   AsBytes(c),
				   AsBytes(pk).first<crypto_box_PUBLICKEYBYTES>(),
				   AsBytes(sk).first<crypto_box_SECRETKEYBYTES>()) != 0)
		return 0;

	lua_pushlstring(L, m.get(), c.size() - crypto_box_SEALBYTES);
	return 1;
}

} // namespace Lua
