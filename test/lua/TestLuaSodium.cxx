// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "lua/Assert.hxx"
#include "lua/Error.hxx"
#include "lua/State.hxx"
#include "lua/StringView.hxx"
#include "lua/Util.hxx"
#include "lua/sodium/Init.hxx"

#include <gtest/gtest.h>

extern "C" {
#include <lauxlib.h>
#include <lualib.h>
}

using std::string_view_literals::operator""sv;

TEST(LuaSodium, Hex)
{
	const Lua::State main{luaL_newstate()};
	lua_State *const L = main.get();
	const Lua::ScopeCheckStack check_stack{L};
	Lua::InitSodium(L);

	Lua::SetGlobal(L, "bin", "AB\0\xff\xfe"sv);

	if (luaL_dostring(L, R"(
hex = sodium.bin2hex(bin)
bin2 = sodium.hex2bin(hex)
empty_hex = sodium.bin2hex("")
bin3 = sodium.hex2bin("007f80ff")
empty_bin = sodium.hex2bin("")
bad_bin1 = sodium.hex2bin("xx")
bad_bin2 = sodium.hex2bin("410")
)"))
		throw Lua::PopError(L);

	lua_getglobal(L, "hex");
	ASSERT_TRUE(lua_isstring(L, -1));
	ASSERT_EQ(Lua::ToStringView(L, -1), "414200fffe"sv);
	lua_pop(L, 1);

	lua_getglobal(L, "bin2");
	ASSERT_TRUE(lua_isstring(L, -1));
	ASSERT_EQ(Lua::ToStringView(L, -1), "AB\0\xff\xfe"sv);
	lua_pop(L, 1);

	lua_getglobal(L, "empty_hex");
	ASSERT_TRUE(lua_isstring(L, -1));
	ASSERT_EQ(Lua::ToStringView(L, -1), ""sv);
	lua_pop(L, 1);

	lua_getglobal(L, "bin3");
	ASSERT_TRUE(lua_isstring(L, -1));
	ASSERT_EQ(Lua::ToStringView(L, -1), "\x00\x7f\x80\xff"sv);
	lua_pop(L, 1);

	lua_getglobal(L, "empty_bin");
	ASSERT_TRUE(lua_isstring(L, -1));
	ASSERT_EQ(Lua::ToStringView(L, -1), ""sv);
	lua_pop(L, 1);

	lua_getglobal(L, "bad_bin1");
	ASSERT_TRUE(lua_isnil(L, -1));
	lua_pop(L, 1);

	lua_getglobal(L, "bad_bin2");
	ASSERT_TRUE(lua_isnil(L, -1));
	lua_pop(L, 1);
}

TEST(LuaSodium, RandomBytes)
{
	const Lua::State main{luaL_newstate()};
	lua_State *const L = main.get();
	const Lua::ScopeCheckStack check_stack{L};
	Lua::InitSodium(L);

	if (luaL_dostring(L, R"(
r1 = sodium.randombytes(1)
r3 = sodium.randombytes(3)
r8 = sodium.randombytes(8)
r1024 = sodium.randombytes(1024)
)"))
		throw Lua::PopError(L);

	lua_getglobal(L, "r1");
	ASSERT_TRUE(lua_isstring(L, -1));
	ASSERT_EQ(Lua::ToStringView(L, -1).size(), 1);
	lua_pop(L, 1);

	lua_getglobal(L, "r3");
	ASSERT_TRUE(lua_isstring(L, -1));
	ASSERT_EQ(Lua::ToStringView(L, -1).size(), 3);
	lua_pop(L, 1);

	lua_getglobal(L, "r8");
	ASSERT_TRUE(lua_isstring(L, -1));
	ASSERT_EQ(Lua::ToStringView(L, -1).size(), 8);
	lua_pop(L, 1);

	lua_getglobal(L, "r1024");
	ASSERT_TRUE(lua_isstring(L, -1));
	ASSERT_EQ(Lua::ToStringView(L, -1).size(), 1024);
	lua_pop(L, 1);
}

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
