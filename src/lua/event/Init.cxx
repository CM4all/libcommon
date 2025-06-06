// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Init.hxx"
#include "Timer.hxx"

namespace Lua {

void
InitEvent(lua_State *L, EventLoop &event_loop) noexcept
{
	InitTimer(L, event_loop);
}

} // namespace Lua
