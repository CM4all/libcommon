// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

struct lua_State;
class EventLoop;

namespace Lua {

void
InitEvent(lua_State *L, EventLoop &event_loop) noexcept;

} // namespace Lua
