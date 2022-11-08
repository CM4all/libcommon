/*
 * Copyright 2022 CM4all GmbH
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

#include "ToJson.hxx"
#include "ForEach.hxx"
#include "StringView.hxx"
#include "util/ScopeExit.hxx"

#include <boost/json/value.hpp>
#include <boost/json/serialize.hpp>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include <stdio.h>

namespace Lua {

static boost::json::string
PointerToJson(const char *prefix, const void *ptr) noexcept
{
	char buffer[32];
	snprintf(buffer, sizeof(buffer), "%s:%p", prefix, ptr);
	return buffer;
}

static boost::json::string
UserDataToJson(lua_State *L, int idx) noexcept
{
	return PointerToJson("userdata", lua_touserdata(L, idx));
}

static boost::json::string
FunctionToJson(lua_State *L, int idx) noexcept
{
	return PointerToJson("cfunction",
			     (const void *)lua_tocfunction(L, idx));
}

static boost::json::string
ThreadToJson(lua_State *L, int idx) noexcept
{
	return PointerToJson("thread", lua_tothread(L, idx));
}

static boost::json::object
TableToJson(lua_State *L, int idx) noexcept
{
	// TODO array?

	boost::json::object o;

	ForEach(L, idx, [L, &o](auto key_idx, auto value_idx){
		auto value = ToJson(L, GetStackIndex(value_idx));

		lua_pushvalue(L, GetStackIndex(key_idx));
		AtScopeExit(L) { lua_pop(L, 1); };

		const auto key = ToStringView(L, -1);
		o.emplace(key, std::move(value));
	});

	return o;
}

boost::json::value
ToJson(lua_State *L, int idx) noexcept
{
	switch (lua_type(L, idx)) {
	case LUA_TNIL:
		return {};

	case LUA_TBOOLEAN:
		return lua_toboolean(L, idx);

	case LUA_TLIGHTUSERDATA:
	case LUA_TUSERDATA:
		return UserDataToJson(L, idx);

	case LUA_TNUMBER:
		// TODO what about floating point?
		return lua_tointeger(L, idx);

	case LUA_TSTRING:
		return ToStringView(L, idx);

	case LUA_TTABLE:
		return TableToJson(L, idx);

	case LUA_TFUNCTION:
		return FunctionToJson(L, idx);

	case LUA_TTHREAD:
		return ThreadToJson(L, idx);
	}

	// TODO what now?
	return {};
}

static int
ToJson(lua_State *L)
{
	if (lua_gettop(L) < 1)
		return luaL_error(L, "Not enough parameters");

	if (lua_gettop(L) > 1)
		return luaL_error(L, "Too many parameters");

	const auto json = boost::json::serialize(ToJson(L, 1));
	lua_pushlstring(L, json.data(), json.size());
	return 1;
}

void
InitToJson(lua_State *L) noexcept
{
	lua_pushcfunction(L, ToJson);
	lua_setglobal(L, "to_json");
}

} // namespace Lua
