// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Connection.hxx"
#include "Result.hxx"
#include "SResult.hxx"
#include "lua/Assert.hxx"
#include "lua/Class.hxx"
#include "lua/Error.hxx"
#include "lua/ForEach.hxx"
#include "lua/StackIndex.hxx"
#include "lua/StringView.hxx"
#include "lua/Util.hxx"
#include "lib/mariadb/BindVector.hxx"
#include "lib/mariadb/Connection.hxx"
#include "lib/mariadb/Statement.hxx"
#include "util/ScopeExit.hxx"
#include "util/StringAPI.hxx"

extern "C" {
#include <lauxlib.h>
}

namespace Lua::MariaDB {

struct ArgError { const char *extramsg; };

static constexpr char lua_connection[] = "MariaDB_Connection";
using LuaConnection = Lua::Class<MysqlConnection, lua_connection>;

static void
BindStringValue(lua_State *L, int value_idx,
		MYSQL_BIND &bind, unsigned long &length, my_bool &is_null)
{
	size_t _length;
	const char *s = lua_tolstring(L, value_idx, &_length);
	bind.buffer_type = MYSQL_TYPE_STRING;
	bind.buffer = const_cast<char *>(s);
	length = _length;
	is_null = false;
}

static void
BindIntegerValue(lua_State *L, int value_idx,
		 MYSQL_BIND &bind, unsigned long &length, my_bool &is_null,
		 long long &long_long)
{
	long_long = lua_tointeger(L, value_idx);
	bind.buffer_type = MYSQL_TYPE_LONGLONG;
	bind.buffer = (char *)&long_long;
	length = sizeof(long_long);
	is_null = false;
}

static void
BindValue(lua_State *L, int value_idx,
	  MYSQL_BIND &bind, unsigned long &length, my_bool &is_null,
	  long long &long_long)
{
	switch (lua_type(L, value_idx)) {
	case LUA_TNIL:
		break;

	case LUA_TSTRING:
		BindStringValue(L, value_idx, bind, length, is_null);
		break;

	case LUA_TNUMBER:
		// TODO what about floating point?
		BindIntegerValue(L, value_idx, bind, length, is_null,
				 long_long);
		break;

	default:
		throw ArgError{"Unsupported query parameter type"};
	}
}

static void
BindTable(lua_State *L, auto table_idx,
	  MysqlBindVector &bind, long long *long_longs,
	  const std::size_t n)
{
	std::fill_n(bind.lengths.get(), n, 0);
	std::fill_n(bind.is_nulls.get(), n, true);

	ForEach(L, table_idx, [L, &bind, long_longs, n](auto key_idx, auto value_idx){
		if (!lua_isnumber(L, GetStackIndex(key_idx)))
			throw ArgError{"Bad key type"};

		const int key = lua_tointeger(L, GetStackIndex(key_idx));
		const std::size_t i = std::size_t(key) - 1;
		if (key < 1 || i >= n)
			throw ArgError{"Bad key value"};

		BindValue(L, GetStackIndex(value_idx),
			  bind.binds[i], bind.lengths[i], bind.is_nulls[i],
			  long_longs[i]);
	});
}

static int
Execute(lua_State *L, MysqlConnection &c, std::string_view sql)
{
	c.Query(sql);

	auto result = c.StoreResult();

	if (c.NextResult()) {
		/* there is more than one result - return all results
		   in a Lua array */
		int n = 0;
		lua_newtable(L);

		NewResult(L, std::move(result));
		lua_rawseti(L, -2, ++n);

		try {
			do {
				NewResult(L, c.StoreResult());
				lua_rawseti(L, -2, ++n);
			} while (c.NextResult());
		} catch (...) {
			lua_pop(L, 1);
			throw;
		}

		return 1;
	} else {
		/* only one result: return it directly */
		return NewResult(L, std::move(result));
	}
}

static int
Execute(lua_State *L, MysqlConnection &c, std::string_view sql, int params_idx)
{
	auto stmt = c.Prepare(sql);

	const std::size_t n = stmt.GetParamCount();
	MysqlBindVector bind{n};
	std::unique_ptr<long long[]> long_longs{new long long[n]};

	if (!lua_istable(L, params_idx))
		luaL_argerror(L, params_idx, "table expected");

	try {
		BindTable(L, params_idx, bind, long_longs.get(), n);
	} catch (const ArgError &e) {
		luaL_argerror(L, params_idx, e.extramsg);
	}

	stmt.BindParam(bind.binds.get());
	stmt.Execute();

	if (stmt.GetFieldCount() == 0)
		return 0;

	return NewSResult(L, std::move(stmt));
}

static int
Execute(lua_State *L)
{
	const auto top = lua_gettop(L);
	if (top < 2)
		return luaL_error(L, "Not enough parameters");

	auto &c = LuaConnection::Cast(L, 1);

	if (!lua_isstring(L, 2))
		luaL_argerror(L, 2, "string expected");

	const auto sql = ToStringView(L, 2);

	if (top >= 3) {
		if (top > 3)
			return luaL_error(L, "Too many parameters");

		try {
			return Execute(L, c, sql, 3);
		} catch (...) {
			RaiseCurrent(L);
		}
	} else {
		try {
			return Execute(L, c, sql);
		} catch (...) {
			RaiseCurrent(L);
		}
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
	unsigned long clientflag = 0;

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
	} else if (StringIsEqual(name, "multi_statements")) {
		if (!lua_isboolean(L, value_idx))
			throw ArgError{"Bad multi_statements value"};

		if (lua_toboolean(L, value_idx))
			clientflag |= CLIENT_MULTI_STATEMENTS;
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
		/* explicitly using "this->" to work around bogus
		   clang16 -Wunused-lambda-capture warning */
		this->Apply(L, GetStackIndex(key_idx), GetStackIndex(value_idx));
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
	} catch (const ArgError &e) {
		luaL_argerror(L, 2, e.extramsg);
	}

	auto *c = LuaConnection::New(L);

	try {
		c->Connect(params.host, params.user, params.passwd,
			   params.db, params.port,
			   params.unix_socket,
			   params.clientflag);
	} catch (...) {
		lua_pop(L, 1);
		RaiseCurrent(L);
	}

	return 1;
}

} // namespace Lua::MariaDB
