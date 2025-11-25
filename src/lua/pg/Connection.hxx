// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

struct lua_State;
class EventLoop;

namespace Pg { struct Config; }

namespace Lua {

void
InitPgConnection(lua_State *L) noexcept;

void
NewPgConnection(struct lua_State *L, EventLoop &event_loop,
		Pg::Config &&config) noexcept;

} // namespace Lua
