// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Result.hxx"
#include "lib/mariadb/Result.hxx"
#include "lua/Assert.hxx"
#include "lua/Class.hxx"
#include "lua/Error.hxx"
#include "util/StringAPI.hxx"

#include <memory>

namespace Lua::MariaDB {

static constexpr char lua_result[] = "MariaDB_Result";
using LuaResult = Lua::Class<MysqlResult, lua_result>;

static int
Close(lua_State *L)
{
	if (lua_gettop(L) > 1)
		return luaL_error(L, "Too many parameters");

	auto &result = LuaResult::Cast(L, 1);
	result = {};
	return 0;
}

static int
Fetch(lua_State *L)
try {
	auto &result = LuaResult::Cast(L, 1);
	if (!result)
		throw std::runtime_error{"Result was already closed"};

	bool numerical = true;
	const char *mode = luaL_optstring(L, 3, "a");
	if (StringIsEqual(mode, "a"))
		numerical = false;
	else if (!StringIsEqual(mode, "n"))
		luaL_argerror(L, 3, "Bad mode");

	auto *row = result.FetchRow();
	if (row == nullptr)
		return 0;

	const auto *lengths = result.FetchLengths();

	if (lua_gettop(L) >= 2)
		Push(L, StackIndex{2});
	else
		lua_newtable(L);

	const std::size_t n_fields = result->field_count;
	for (std::size_t i = 0; i < n_fields; ++i) {
		if (numerical)
			Push(L, (lua_Integer)i + 1);
		else
			Push(L, result->fields[i].name);

		if (row[i] == nullptr) {
			Push(L, nullptr);
		} else {
			Push(L, std::string_view{row[i], lengths[i]});
		}

		lua_settable(L, -3);
	}

	return 1;
} catch (...) {
	RaiseCurrent(L);
}

static constexpr struct luaL_Reg result_methods[] = {
	{"close", Close},
	{"fetch", Fetch},
	{nullptr, nullptr}
};

void
InitResult(lua_State *L)
{
	const ScopeCheckStack check_stack{L};

	LuaResult::Register(L);
	luaL_newlib(L, result_methods);
	lua_setfield(L, -2, "__index");
	lua_pop(L, 1);
}

int
NewResult(lua_State *L, MysqlResult &&result)
{
	try {
		LuaResult::New(L, std::move(result));
		return 1;
	} catch (...) {
		RaiseCurrent(L);
	}
}

} // namespace Lua::MariaDB
