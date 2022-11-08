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

#include "Connection.hxx"
#include "Result.hxx"
#include "lua/Assert.hxx"
#include "lua/Class.hxx"
#include "lua/Error.hxx"
#include "lua/ForEach.hxx"
#include "lua/StackIndex.hxx"
#include "lua/StringView.hxx"
#include "lua/Util.hxx"
#include "lib/mariadb/Connection.hxx"
#include "lib/mariadb/Statement.hxx"
#include "util/ScopeExit.hxx"
#include "util/StringAPI.hxx"

extern "C" {
#include <lauxlib.h>
}

namespace Lua::MariaDB {

static constexpr char lua_connection[] = "MariaDB_Connection";
using LuaConnection = Lua::Class<MysqlConnection, lua_connection>;

static int
Execute(lua_State *L)
{
	if (lua_gettop(L) < 2)
		return luaL_error(L, "Not enough parameters");

	auto &c = LuaConnection::Cast(L, 1);

	if (!lua_isstring(L, 2))
		luaL_argerror(L, 2, "string expected");

	const auto sql = ToStringView(L, 2);

	try {
		auto stmt = c.Prepare(sql);
		// TODO bind
		stmt.Execute();

		if (stmt.GetFieldCount() == 0)
			return 0;

		return NewResult(L, std::move(stmt));
	} catch (...) {
		RaiseCurrent(L);
	}
}

static constexpr struct luaL_Reg connection_methods[] = {
	{"execute", Execute},
	{nullptr, nullptr}
};

void
InitConnection(lua_State *L)
{
	const ScopeCheckStack check_stack{L};

	LuaConnection::Register(L);
	luaL_newlib(L, connection_methods);
	lua_setfield(L, -2, "__index");
	lua_pop(L, 1);
}

struct Params {
	const char *host = nullptr, *user = nullptr, *passwd = nullptr;
	const char *db = nullptr;
	const char *unix_socket = nullptr;
	unsigned port = 3306;

	struct ArgError { const char *extramsg; };

	void Apply(lua_State *L, const char *name, int value_idx);
	void Apply(lua_State *L, int key_idx, int value_idx);
	void ApplyTable(lua_State *L, auto table_idx);
};

inline void
Params::Apply(lua_State *L, const char *name, int value_idx)
{
	const ScopeCheckStack check_stack{L};

	if (StringIsEqual(name, "host")) {
		if (!lua_isstring(L, value_idx))
			throw ArgError{"Bad host type"};

		host = lua_tostring(L, value_idx);
	} else if (StringIsEqual(name, "user")) {
		if (!lua_isstring(L, value_idx))
			throw ArgError{"Bad user type"};

		user = lua_tostring(L, value_idx);
	} else if (StringIsEqual(name, "passwd")) {
		if (!lua_isstring(L, value_idx))
			throw ArgError{"Bad passwd type"};

		passwd = lua_tostring(L, value_idx);
	} else if (StringIsEqual(name, "db")) {
		if (!lua_isstring(L, value_idx))
			throw ArgError{"Bad db type"};

		db = lua_tostring(L, value_idx);
	} else if (StringIsEqual(name, "unix_socket")) {
		if (!lua_isstring(L, value_idx))
			throw ArgError{"Bad unix_socket type"};

		unix_socket = lua_tostring(L, value_idx);
	} else if (StringIsEqual(name, "port")) {
		if (!lua_isnumber(L, value_idx))
			throw ArgError{"Bad port type"};

		const auto _port = lua_tointeger(L, value_idx);
		if (_port < 1 || _port > 65535)
			throw ArgError{"Bad port value"};

		port = (unsigned)_port;
	} else
		throw ArgError{name};
}

inline void
Params::Apply(lua_State *L, int key_idx, int value_idx)
{
	const ScopeCheckStack check_stack{L};

	if (!lua_isstring(L, key_idx))
		throw ArgError{"Bad key type"};

	const char *key = lua_tostring(L, key_idx);
	Apply(L, key, value_idx);
}

inline void
Params::ApplyTable(lua_State *L, auto table_idx)
{
	ForEach(L, table_idx, [this, L](auto key_idx, auto value_idx){
		Apply(L, GetStackIndex(key_idx), GetStackIndex(value_idx));
	});
}

int
NewConnection(lua_State *L)
{
	if (lua_gettop(L) != 2)
		return luaL_error(L, "Invalid parameter count");

	if (!lua_istable(L, 2))
		luaL_argerror(L, 2, "table expected");

	Params params;
	try {
		params.ApplyTable(L, 2);
	} catch (const Params::ArgError &e) {
		luaL_argerror(L, 2, e.extramsg);
	}

	auto *c = LuaConnection::New(L);

	try {
		c->Connect(params.host, params.user, params.passwd,
			   params.db, params.port,
			   params.unix_socket);
	} catch (...) {
		lua_pop(L, 1);
		RaiseCurrent(L);
	}

	return 1;
}

} // namespace Lua::MariaDB
