// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "lua/ForEach.hxx"
#include "lua/Assert.hxx"
#include "lua/State.hxx"
#include "lua/Util.hxx"

#include <gtest/gtest.h>

extern "C" {
#include <lauxlib.h>
#include <lualib.h>
}

using namespace Lua;

TEST(ForEach, Empty)
{
	const State main{luaL_newstate()};
	const auto L = main.get();
	const ScopeCheckStack check_stack{L};

	lua_newtable(L);

	ForEach(L, RelativeStackIndex{-1}, [](auto, auto){
		FAIL();
	});

	lua_pop(L, 1);
}

TEST(ForEach, One)
{
	const State main{luaL_newstate()};
	const auto L = main.get();
	const ScopeCheckStack check_stack{L};

	lua_newtable(L);
	RawSet(L, RelativeStackIndex{-1}, 42, "foo");

	unsigned n = 0;
	ForEach(L, RelativeStackIndex{-1}, [L, &n](auto key_idx, auto value_idx){
		EXPECT_EQ(n, 0);
		++n;

		EXPECT_TRUE(lua_isnumber(L, GetStackIndex(key_idx)));
		EXPECT_EQ(lua_tointeger(L, GetStackIndex(key_idx)), 42);

		EXPECT_TRUE(lua_isstring(L, GetStackIndex(value_idx)));
		EXPECT_STREQ(lua_tostring(L, GetStackIndex(value_idx)), "foo");
	});

	EXPECT_EQ(n, 1);

	lua_pop(L, 1);
}

TEST(ForEach, Throw)
{
	const State main{luaL_newstate()};
	const auto L = main.get();
	const ScopeCheckStack check_stack{L};

	lua_newtable(L);
	RawSet(L, RelativeStackIndex{-1}, 42, "foo");

	try {
		ForEach(L, RelativeStackIndex{-1}, [](auto, auto){
			throw 42;
		});

		FAIL();
	} catch (int e) {
		EXPECT_EQ(e, 42);
	}

	lua_pop(L, 1);
}

TEST(ForEach, Error)
{
	const State main{luaL_newstate()};
	const auto L = main.get();
	const ScopeCheckStack check_stack{L};

	lua_newtable(L);
	RawSet(L, RelativeStackIndex{-1}, 42, "foo");

	try {
		ForEach(L, RelativeStackIndex{-1}, [L](auto, auto){
			lua_pushstring(L, "error");
			lua_error(L);
		});

		FAIL();
	} catch (...) {
		EXPECT_FALSE(std::exception_ptr());
		EXPECT_TRUE(lua_isstring(L, -1));
		EXPECT_STREQ(lua_tostring(L, -1), "error");
		lua_pop(L, 1);
	}

	lua_pop(L, 1);
}

TEST(ForEach, AuxError)
{
	const State main{luaL_newstate()};
	const auto L = main.get();
	const ScopeCheckStack check_stack{L};

	lua_newtable(L);
	RawSet(L, RelativeStackIndex{-1}, 42, "foo");

	try {
		ForEach(L, RelativeStackIndex{-1}, [L](auto, auto){
			luaL_error(L, "error");
		});

		FAIL();
	} catch (...) {
		EXPECT_FALSE(std::exception_ptr());
		EXPECT_TRUE(lua_isstring(L, -1));
		EXPECT_STREQ(lua_tostring(L, -1), "error");
		lua_pop(L, 1);
	}

	lua_pop(L, 1);
}
