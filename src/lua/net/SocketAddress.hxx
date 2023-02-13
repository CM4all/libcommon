// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

struct lua_State;
class SocketAddress;
class AllocatedSocketAddress;

namespace Lua {

void
InitSocketAddress(lua_State *L);

void
NewSocketAddress(lua_State *L, SocketAddress src);

void
NewSocketAddress(lua_State *L, AllocatedSocketAddress src);

[[nodiscard]]
SocketAddress
GetSocketAddress(lua_State *L, int idx);

[[nodiscard]]
AllocatedSocketAddress
ToSocketAddress(lua_State *L, int idx, int default_port);

} // namespace Lua
