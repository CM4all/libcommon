// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

struct lua_State;

namespace Lua {

/**
 * Register the constructor control_client:new() which is wrapper for
 * BengControl::Client.
 */
void
InitControlClient(lua_State *L);

} // namespace Lua
