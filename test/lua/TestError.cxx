// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "lua/Error.hxx"
#include "lua/Assert.hxx"
#include "lua/State.hxx"

#include <gtest/gtest.h>

extern "C" {
#include <lauxlib.h>
#include <lualib.h>
}

#include <stdexcept>

namespace {

static int
ThrowStringError(lua_State *L)
{
	lua_pushstring(L, "foo");
	return lua_error(L);
}

static int
ThrowTableError(lua_State *L)
{
	lua_newtable(L);
	return lua_error(L);
}

static int
RaiseCurrentCppException(lua_State *L)
{
	try {
		throw std::runtime_error{"foo"};
	} catch (...) {
		Lua::RaiseCurrent(L);
	}
}

} // anonymous namespace

TEST(LuaError, PopErrorString)
{
	const Lua::State main{luaL_newstate()};
	const auto L = main.get();
	const Lua::ScopeCheckStack check_stack{L};

	lua_pushcfunction(L, ThrowStringError);
	ASSERT_EQ(lua_pcall(L, 0, 0, 0), LUA_ERRRUN);
	ASSERT_EQ(lua_gettop(L), 1);

	const auto error = Lua::PopError(L);
	EXPECT_STREQ(error.what(), "foo");
	EXPECT_EQ(lua_gettop(L), 0);
}

TEST(LuaError, PopErrorNonString)
{
	const Lua::State main{luaL_newstate()};
	const auto L = main.get();
	const Lua::ScopeCheckStack check_stack{L};

	lua_pushcfunction(L, ThrowTableError);
	ASSERT_EQ(lua_pcall(L, 0, 0, 0), LUA_ERRRUN);
	ASSERT_EQ(lua_gettop(L), 1);

	const auto error = Lua::PopError(L);
	EXPECT_STREQ(error.what(), "table");
	EXPECT_EQ(lua_gettop(L), 0);
}

TEST(LuaError, PushException)
{
	const Lua::State main{luaL_newstate()};
	const auto L = main.get();
	const Lua::ScopeCheckStack check_stack{L};

	Lua::Push(L, std::make_exception_ptr(std::runtime_error{"foo"}));
	ASSERT_TRUE(lua_isstring(L, -1));
	EXPECT_STREQ(lua_tostring(L, -1), "foo");
	lua_pop(L, 1);
}

TEST(LuaError, RaiseCurrentCppException)
{
	const Lua::State main{luaL_newstate()};
	const auto L = main.get();
	const Lua::ScopeCheckStack check_stack{L};

	lua_pushcfunction(L, RaiseCurrentCppException);
	ASSERT_EQ(lua_pcall(L, 0, 0, 0), LUA_ERRRUN);
	ASSERT_EQ(lua_gettop(L), 1);
	ASSERT_TRUE(lua_isstring(L, -1));
	EXPECT_STREQ(lua_tostring(L, -1), "foo");
	lua_pop(L, 1);
}
