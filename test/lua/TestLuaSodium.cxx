// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "lua/Assert.hxx"
#include "lua/Error.hxx"
#include "lua/State.hxx"
#include "lua/StringView.hxx"
#include "lua/sodium/Init.hxx"

#include <gtest/gtest.h>

extern "C" {
#include <lauxlib.h>
#include <lualib.h>
}

using std::string_view_literals::operator""sv;

TEST(LuaSodium, Box)
{
	const Lua::State main{luaL_newstate()};
	lua_State *const L = main.get();
	const Lua::ScopeCheckStack check_stack{L};
	Lua::InitSodium(L);

	if (luaL_dostring(L, R"(
pk, sk = sodium.crypto_box_keypair()
pk2 = sodium.crypto_scalarmult_base(sk)
ciphertext = sodium.crypto_box_seal('hello world', pk)
message = sodium.crypto_box_seal_open(ciphertext, pk, sk)
)"))
		throw Lua::PopError(L);

	lua_getglobal(L, "message");
	ASSERT_TRUE(lua_isstring(L, -1));
	ASSERT_EQ(Lua::ToStringView(L, -1), "hello world"sv);
	lua_pop(L, 1);

	lua_getglobal(L, "pk");
	lua_getglobal(L, "pk2");
	ASSERT_TRUE(lua_isstring(L, -2));
	ASSERT_TRUE(lua_isstring(L, -1));
	ASSERT_EQ(Lua::ToStringView(L, -1), Lua::ToStringView(L, -2));
	lua_pop(L, 2);
}
