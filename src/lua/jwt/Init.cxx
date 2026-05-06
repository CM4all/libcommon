// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Init.hxx"
#include "lua/CheckArg.hxx"
#include "lua/Util.hxx"
#include "lua/sodium/CheckKey.hxx"
#include "lua/json/ToJson.hxx"
#include "lib/sodium/Base64Alloc.hxx"
#include "jwt/EdDSA.hxx"
#include "util/AllocatedString.hxx"

#include <nlohmann/json.hpp>

using std::string_view_literals::operator""sv;

namespace Lua {

static int
SignJwt(lua_State *L)
{
	if (lua_gettop(L) < 3)
		return luaL_error(L, "Not enough parameters");

	if (lua_gettop(L) > 3)
		return luaL_error(L, "Too many parameters");

	const auto sk = CheckSodiumKey<CryptoSignSecretKeyView>(L, 1, "Malformed secret key");

	luaL_checktype(L, 2, LUA_TTABLE);
	auto header = ToJson(L, 2);

	luaL_checktype(L, 3, LUA_TTABLE);
	const auto payload = ToJson(L, 3);

	header["alg"sv] = "EdDSA"sv;
	header["typ"sv] = "JWT"sv;

	const auto header_b64 = UrlSafeBase64(header.dump());
	const auto payload_b64 = UrlSafeBase64(payload.dump());
	const auto signature = JWT::SignEdDSA(sk, header_b64, payload_b64);

	lua_pushstring(L, AllocatedString{header_b64, "."sv, payload_b64, "."sv, signature}.c_str());
	return 1;
}

void
InitJwt(lua_State *L) noexcept
{
	lua_newtable(L);
	SetTable(L, RelativeStackIndex{-1}, "sign", SignJwt);
	lua_setglobal(L, "jwt");
}

} // namespace Lua
