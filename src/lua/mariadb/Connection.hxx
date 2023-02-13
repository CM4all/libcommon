// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

struct lua_State;

namespace Lua::MariaDB {

void
InitConnection(lua_State *L);

int
NewConnection(lua_State *L);

} // namespace Lua
