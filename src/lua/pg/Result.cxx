// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

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

	int Fetch(lua_State *L);
};

static constexpr char lua_pg_result_class[] = "pg.Result";
using PgResultClass = Lua::Class<PgResult, lua_pg_result_class>;

static constexpr struct luaL_Reg lua_pg_result_methods [] = {
	{"fetch", PgResultClass::WrapMethod<&PgResult::Fetch>()},
	{nullptr, nullptr}
};

inline int
PgResult::Fetch(lua_State *L)
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
			Push(L, (lua_Integer)i + 1);
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
	luaL_newlib(L, lua_pg_result_methods);
	lua_setfield(L, -2, "__index");
	lua_pop(L, 1);
}

void
NewPgResult(struct lua_State *L, Pg::Result result) noexcept
{
	PgResultClass::New(L, std::move(result));
}

} // namespace Lua
