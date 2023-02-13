// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

struct lua_State;
namespace Pg { class Result; }

namespace Lua {

void
InitPgResult(lua_State *L) noexcept;

void
NewPgResult(struct lua_State *L, Pg::Result result) noexcept;

} // namespace Lua
