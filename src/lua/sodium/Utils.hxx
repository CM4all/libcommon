// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

struct lua_State;

namespace Lua {

int
bin2hex(lua_State *L);

int
hex2bin(lua_State *L);

} // namespace Lua
