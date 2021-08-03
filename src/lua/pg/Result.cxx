/*
 * Copyright 2021 CM4all GmbH
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
#include "lua/Class.hxx"
#include "lua/Util.hxx"
#include "pg/Result.hxx"
#include "util/StringAPI.hxx"

extern "C" {
#include <lauxlib.h>
}

namespace Lua {

class PgResult final {
	const Pg::Result result;

	unsigned next_row = 0;

public:
	explicit PgResult(Pg::Result &&_result) noexcept
		:result(std::move(_result)) {}

private:
	static int Fetch(lua_State *L);
	int _Fetch(lua_State *L);

public:
	static constexpr struct luaL_Reg methods [] = {
		{"fetch", Fetch},
		{nullptr, nullptr}
	};
};

static constexpr char lua_pg_result_class[] = "pg.Result";
using PgResultClass = Lua::Class<PgResult, lua_pg_result_class>;

int
PgResult::Fetch(lua_State *L)
{
	auto &result = PgResultClass::Cast(L, 1);
	return result._Fetch(L);
}

inline int
PgResult::_Fetch(lua_State *L)
{
	if (next_row >= result.GetRowCount())
		return 0;

	bool numerical = true;
	const char *mode = luaL_optstring(L, 3, "a");
	if (StringIsEqual(mode, "a"))
		numerical = false;
	else if (!StringIsEqual(mode, "n"))
		luaL_argerror(L, 3, "Bad mode");

	if (lua_gettop(L) >= 2)
		Push(L, StackIndex{2});
	else
		lua_newtable(L);

	const unsigned row = next_row++;
	const unsigned n_columns = result.GetColumnCount();

	for (unsigned i = 0; i < n_columns; ++i) {
		if (numerical)
			Push(L, (int)i + 1);
		else
			Push(L, result.GetColumnName(i));

		if (result.IsValueNull(row, i))
			Push(L, nullptr);
		else
			Push(L, result.GetValueView(row, i));

		lua_settable(L, -3);
	}

	return 1;
}

void
InitPgResult(lua_State *L) noexcept
{
	PgResultClass::Register(L);
	luaL_newlib(L, PgResult::methods);
	lua_setfield(L, -2, "__index");
	lua_pop(L, 1);
}

void
NewPgResult(struct lua_State *L, Pg::Result result) noexcept
{
	PgResultClass::New(L, std::move(result));
}

} // namespace Lua
