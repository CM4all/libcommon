// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Sign.hxx"
#include "CheckKey.hxx"
#include "lua/CheckArg.hxx"
#include "lua/Util.hxx"
#include "lib/sodium/Sign.hxx"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include <memory>

namespace Lua {

int
crypto_sign_keypair(lua_State *L)
{
	if (lua_gettop(L) > 0)
		return luaL_error(L, "Too many parameters");

	CryptoSignPublicKey pk;
	CryptoSignSecretKey sk;
	::crypto_sign_keypair(pk, sk);

	Push(L, pk);
	Push(L, sk);
	return 2;
}

int
crypto_sign_detached(lua_State *L)
{
	if (lua_gettop(L) > 2)
		return luaL_error(L, "Too many parameters");

	const auto m = CheckByteSpan(L, 1);
	const auto sk = CheckSodiumKey<CryptoSignSecretKeyView>(L, 2, "Malformed secret key");

	CryptoSignature sig;

	::crypto_sign_detached(sig, m, sk);

	Push(L, sig);
	return 1;
}

int
crypto_sign_verify_detached(lua_State *L)
{
	if (lua_gettop(L) > 3)
		return luaL_error(L, "Too many parameters");

	const auto sig = CheckByteSpan(L, 1);
	const auto m = CheckByteSpan(L, 2);
	const auto pk = CheckSodiumKey<CryptoSignPublicKeyView>(L, 3, "Malformed public key");

	if (sig.size() != crypto_sign_BYTES)
		return 0;

	lua_pushboolean(L, ::crypto_sign_verify_detached(sig.first<crypto_sign_BYTES>(), m, pk) == 0);
	return 1;
}

} // namespace Lua
