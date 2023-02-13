// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

struct lua_State;
class MysqlResult;

namespace Lua::MariaDB {

void
InitResult(lua_State *L);

int
NewResult(lua_State *L, MysqlResult &&result);

} // namespace Lua::MariaDB
