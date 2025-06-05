// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

struct lua_State;
class MysqlStatement;

namespace Lua::MariaDB {

void
InitSResult(lua_State *L);

int
NewSResult(lua_State *L, MysqlStatement &&stmt);

} // namespace Lua::MariaDB
