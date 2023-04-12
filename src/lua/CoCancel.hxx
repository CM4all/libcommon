// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

struct lua_State;

namespace Lua {

/**
 * If the state is suspended (#LUA_YIELD), attempt to close the
 * blocking operation by invoking its `__close` metamethod.
 *
 * This requires that prior to calling lua_yield(), an object with a
 * `__close` metamethod was pushed on the stack.
 *
 * @return true if `__close` has been called
 */
bool
CoCancel(lua_State *L);

} // namespace Lua
