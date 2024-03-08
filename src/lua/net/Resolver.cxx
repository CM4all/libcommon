// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Resolver.hxx"
#include "SocketAddress.hxx"
#include "lua/Error.hxx"
#include "lua/Util.hxx"
#include "lua/PushCClosure.hxx"
#include "net/AddressInfo.hxx"
#include "net/AllocatedSocketAddress.hxx"
#include "net/Resolver.hxx"

extern "C" {
#include <lauxlib.h>
}

namespace Lua {

static int
l_resolve(lua_State *L)
{
	const auto &hints = *(struct addrinfo *)lua_touserdata(L, lua_upvalueindex(1));
	const uint_least16_t default_port = lua_tointeger(L, lua_upvalueindex(2));

	if (lua_gettop(L) != 1)
		return luaL_error(L, "Invalid parameter count");

	const char *s = luaL_checkstring(L, 1);

	if (*s == '/' || *s == '@') {
		AllocatedSocketAddress address;
		address.SetLocal(s);
		NewSocketAddress(L, std::move(address));
		return 1;
	}

	AddressInfoList ai;

	try {
		ai = Resolve(s, default_port, &hints);
	} catch (...) {
		RaiseCurrent(L);
	}

	NewSocketAddress(L, std::move(ai.GetBest()));
	return 1;
}

void
PushResolveFunction(lua_State *L, const struct addrinfo &hints,
		    uint_least16_t default_port)
{
	Push(L, MakeCClosure(l_resolve,
			     LightUserData{const_cast<struct addrinfo *>(&hints)},
			     static_cast<lua_Integer>(default_port)));
}

} // namespace Lua
