// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Init.hxx"
#include "Array.hxx"
#include "Connection.hxx"
#include "Result.hxx"
#include "lua/LightUserData.hxx"
#include "lua/Util.hxx"
#include "lua/PushCClosure.hxx"
#include "pg/Config.hxx"

extern "C" {
#include <lauxlib.h>
}

#include <cassert>

namespace Lua {

static int
NewPg(lua_State *L)
{
	auto &event_loop = *(EventLoop *)lua_touserdata(L, lua_upvalueindex(1));
	auto *const main_L = lua_tothread(L, lua_upvalueindex(2));
	assert(main_L != nullptr);

	if (lua_gettop(L) > 3)
		return luaL_error(L, "Too many parameters");

	const char *conninfo = luaL_checkstring(L, 2);
	const char *schema = luaL_optstring(L, 3, "");

	NewPgConnection(L, main_L, event_loop,
			{.connect = conninfo, .schema = schema});
	return 1;
}

void
InitPg(lua_State *L, EventLoop &event_loop)
{
	InitPgConnection(L);
	InitPgResult(L);

	lua_newtable(L);
	SetTable(L, RelativeStackIndex{-1}, "new",
		 MakeCClosure(NewPg, LightUserData{&event_loop},
			      CurrentThread{}));
	SetTable(L, RelativeStackIndex{-1}, "encode_array", EncodeArray);
	SetTable(L, RelativeStackIndex{-1}, "decode_array", DecodeArray);
	lua_setglobal(L, "pg");
}

} // namespace Lua
