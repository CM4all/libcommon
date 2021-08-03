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

#include "Init.hxx"
#include "Stock.hxx"
#include "Result.hxx"
#include "lua/Util.hxx"
#include "lua/PushCClosure.hxx"

extern "C" {
#include <lauxlib.h>
}

namespace Lua {

static int
NewPg(lua_State *L)
{
	auto &event_loop = *(EventLoop *)lua_touserdata(L, lua_upvalueindex(1));

	if (lua_gettop(L) > 3)
		return luaL_error(L, "Too many parameters");

	const char *conninfo = luaL_checkstring(L, 2);
	const char *schema = luaL_optstring(L, 3, "");

	constexpr unsigned limit = 4;
	constexpr unsigned max_idle = 1;

	NewPgStock(L, event_loop, conninfo, schema, limit, max_idle);
	return 1;
}

void
InitPg(lua_State *L, EventLoop &event_loop)
{
	InitPgStock(L);
	InitPgResult(L);

	lua_newtable(L);
	SetTable(L, RelativeStackIndex{-1}, "new",
		 Lua::MakeCClosure(NewPg,
				   Lua::LightUserData(&event_loop)));
	lua_setglobal(L, "pg");
}

} // namespace Lua
