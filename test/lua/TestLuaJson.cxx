// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "lua/json/Init.hxx"
#include "lua/json/Push.hxx"
#include "lua/json/ToJson.hxx"
#include "lua/Assert.hxx"
#include "lua/Error.hxx"
#include "lua/State.hxx"
#include "lua/StringView.hxx"
#include "lua/Util.hxx"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

extern "C" {
#include <lauxlib.h>
#include <lualib.h>
}

using std::string_view_literals::operator""sv;

TEST(LuaJson, All)
{
	const Lua::State main{luaL_newstate()};
	lua_State *const L = main.get();
	const Lua::ScopeCheckStack check_stack{L};
	Lua::InitJson(L);

	const nlohmann::json j{
		{"foo"sv, "bar"sv},
		{"x"sv, 42},
		{"y"sv, {
			{"a"sv, "b"sv},
			{"c"sv, 3},
			// TODO array support {"d"sv, {"e"sv, "f"sv, "g"sv}},
		}},
	};

	Lua::Push(L, j);
	lua_setglobal(L, "t");

	Lua::Push(L, static_cast<std::string_view>(j.dump()));
	lua_setglobal(L, "s");

	if (luaL_dostring(L, R"(
s2 = json.dump(t)
t2 = json.parse(s)
)"))
		throw Lua::PopError(L);

	lua_getglobal(L, "s2");
	ASSERT_TRUE(lua_isstring(L, -1));
	EXPECT_EQ(Lua::ToStringView(L, -1), R"({"foo":"bar","x":42,"y":{"a":"b","c":3}})"sv);
	lua_pop(L, 1);

	lua_getglobal(L, "t2");
	ASSERT_TRUE(lua_istable(L, -1));
	EXPECT_EQ(Lua::ToJson(L, -1).dump(), R"({"foo":"bar","x":42,"y":{"a":"b","c":3}})"sv);
	lua_pop(L, 1);
}
