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

#include "SResult.hxx"
#include "lua/Class.hxx"
#include "lua/Error.hxx"
#include "lib/mariadb/BindVector.hxx"
#include "lib/mariadb/Statement.hxx"
#include "util/AllocatedArray.hxx"
#include "util/StringAPI.hxx"

#include <memory>

namespace Lua::MariaDB {

struct SResult {
	MysqlStatement stmt;

	MysqlBindVector bind;

	explicit SResult(MysqlStatement &&_stmt)
		:stmt(std::move(_stmt)),
		 bind(stmt.GetFieldCount()) {
		stmt.BindResult(bind);
		stmt.StoreResult();
	}
};

static constexpr char lua_result[] = "MariaDB_SResult";
using LuaSResult = Lua::Class<SResult, lua_result>;

static int
Close(lua_State *L)
{
	if (lua_gettop(L) > 1)
		return luaL_error(L, "Too many parameters");

	auto &result = LuaSResult::Cast(L, 1);
	result.stmt = {};
	return 0;
}

static int
Fetch(lua_State *L)
try {
	auto &result = LuaSResult::Cast(L, 1);
	if (!result.stmt)
		throw std::runtime_error{"Result was already closed"};

	bool numerical = true;
	const char *mode = luaL_optstring(L, 3, "a");
	if (StringIsEqual(mode, "a"))
		numerical = false;
	else if (!StringIsEqual(mode, "n"))
		luaL_argerror(L, 3, "Bad mode");

	if (!result.stmt.Fetch())
		return 0;

	if (lua_gettop(L) >= 2)
		Push(L, StackIndex{2});
	else
		lua_newtable(L);

	const auto metadata = result.stmt.ResultMetadata();

	AllocatedArray<char> buffer;

	const std::size_t n_fields = result.stmt.GetFieldCount();
	for (std::size_t i = 0; i < n_fields; ++i) {
		if (numerical)
			Push(L, (lua_Integer)i + 1);
		else
			Push(L, metadata->fields[i].name);

		if (result.bind.is_nulls[i]) {
			Push(L, nullptr);
		} else if (const std::size_t length = result.bind.lengths[i];
			   length == 0) {
			Push(L, std::string_view{});
		} else {
			buffer.GrowDiscard(length);
			MYSQL_BIND b{
				.buffer = buffer.data(),
				.buffer_length = length,
			};

			result.stmt.FetchColumn(b, i);
			Push(L, std::string_view{buffer.data(), length});
		}

		lua_settable(L, -3);
	}

	return 1;
} catch (...) {
	RaiseCurrent(L);
}

static constexpr struct luaL_Reg sresult_methods[] = {
	{"close", Close},
	{"fetch", Fetch},
	{nullptr, nullptr}
};

void
InitSResult(lua_State *L)
{
	const ScopeCheckStack check_stack{L};

	LuaSResult::Register(L);
	luaL_newlib(L, sresult_methods);
	lua_setfield(L, -2, "__index");
	lua_pop(L, 1);
}

int
NewSResult(lua_State *L, MysqlStatement &&stmt)
{
	try {
		LuaSResult::New(L, std::move(stmt));
		return 1;
	} catch (...) {
		RaiseCurrent(L);
	}
}

} // namespace Lua::MariaDB
