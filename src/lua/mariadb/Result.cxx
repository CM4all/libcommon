/*
 * Copyright 2021-2022 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
