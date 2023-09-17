// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "SocketAddress.hxx"
#include "lua/Class.hxx"
#include "lua/Util.hxx"
#include "net/AllocatedSocketAddress.hxx"
#include "net/Parser.hxx"
#include "net/ToString.hxx"

namespace Lua {

static constexpr char socket_address_class[] = "SocketAddress";
using LuaSocketAddress =
	Lua::Class<AllocatedSocketAddress, socket_address_class>;

static int
LuaSocketAddressToString(lua_State *L)
{
	if (lua_gettop(L) != 1)
		return luaL_error(L, "Invalid parameters");

	const auto a = GetSocketAddress(L, 1);

	char buffer[256];
	if (!ToString(std::span{buffer}, a))
		return 0;

	Lua::Push(L, buffer);
	return 1;
}

void
InitSocketAddress(lua_State *L)
{
	LuaSocketAddress::Register(L);
	SetTable(L, RelativeStackIndex{-1}, "__tostring",
		 LuaSocketAddressToString);
	lua_pop(L, 1);
}

void
NewSocketAddress(lua_State *L, SocketAddress src)
{
	LuaSocketAddress::New(L, src);
}

void
NewSocketAddress(lua_State *L, AllocatedSocketAddress src)
{
	LuaSocketAddress::New(L, std::move(src));
}

SocketAddress
GetSocketAddress(lua_State *L, int idx)
{
	return LuaSocketAddress::Cast(L, idx);
}

AllocatedSocketAddress
ToSocketAddress(lua_State *L, int idx, int default_port)
{
	if (lua_isstring(L, idx)) {
		const char *s = lua_tostring(L, idx);

		return ParseSocketAddress(s, default_port, false);
	} else {
		return AllocatedSocketAddress{GetSocketAddress(L, idx)};
	}
}

} // namespace Lua
