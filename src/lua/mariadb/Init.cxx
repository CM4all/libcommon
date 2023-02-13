// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Init.hxx"
#include "Connection.hxx"
#include "Result.hxx"
#include "SResult.hxx"
#include "lua/Util.hxx"

namespace Lua::MariaDB {

void
Init(lua_State *L)
{
	InitConnection(L);
	InitResult(L);
	InitSResult(L);

	lua_newtable(L);
	SetTable(L, RelativeStackIndex{-1}, "new", NewConnection);
	lua_setglobal(L, "mariadb");
}

} // namespace Lua::MariaDB
