// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Box.hxx"
#include "CheckKey.hxx"
#include "lua/CheckArg.hxx"
#include "lua/Util.hxx"
#include "lib/sodium/Box.hxx"

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

	Push(L, pk);
	Push(L, sk);
	return 2;
}

int
crypto_box_seal(lua_State *L)
{
	if (lua_gettop(L) > 2)
		return luaL_error(L, "Too many parameters");

	const auto m = CheckByteSpan(L, 1);
	const auto pk = CheckSodiumKey<CryptoBoxPublicKeyView>(L, 2, "Malformed public key");

	std::unique_ptr<char[]> c(new char[m.size() + crypto_box_SEALBYTES]);

	::crypto_box_seal(reinterpret_cast<std::byte *>(c.get()), m, pk);

	lua_pushlstring(L, c.get(), m.size() + crypto_box_SEALBYTES);
	return 1;
}

int
crypto_box_seal_open(lua_State *L)
{
	if (lua_gettop(L) > 3)
		return luaL_error(L, "Too many parameters");

	const auto c = CheckByteSpan(L, 1);
	luaL_argcheck(L, c.size() >= crypto_box_SEALBYTES, 1,
		      "Malformed ciphertext");

	const auto pk = CheckSodiumKey<CryptoBoxPublicKeyView>(L, 2, "Malformed public key");
	const auto sk = CheckSodiumKey<CryptoBoxSecretKeyView>(L, 3, "Malformed secret key");

	std::unique_ptr<char[]> m(new char[c.size() - crypto_box_SEALBYTES]);

	if (::crypto_box_seal_open(reinterpret_cast<std::byte *>(m.get()),
				   c, pk, sk) != 0)
		return 0;

	lua_pushlstring(L, m.get(), c.size() - crypto_box_SEALBYTES);
	return 1;
}

} // namespace Lua
