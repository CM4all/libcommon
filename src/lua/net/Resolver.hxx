// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <cstdint>

struct lua_State;
struct addrinfo;

namespace Lua {

/**
 * Push a function to the Lua stack that takes one string parameter,
 * resolves it and returns a #LuaSocketAddress.  The function will
 * only ever return one address, even if the resolver returns more
 * than one.
 *
 * @param hints hints for getaddrinfo(); the reference must remain
 * valid forever
 * @param default_port the default port numbers for addresses that
 * have one
 */
void
PushResolveFunction(lua_State *L, const struct addrinfo &hints,
		    uint_least16_t default_port);

} // namespace Lua
