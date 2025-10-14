// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

struct lua_State;

namespace Lua {

struct LightUserData;

/**
 * Look up a table in the Lua registry, using a #LightUserData as the
 * registry key.
 *
 * @return true if the table has been pushed on the stack, false if
 * the table does not exist and nothing was pushed on the stack
 */
[[nodiscard]]
bool
GetRegistryTable(lua_State *L, LightUserData key);

/**
 * Like GetRegistryTable(), but create the table if it does not exist.
 */
void
MakeRegistryTable(lua_State *L, LightUserData key);

} // namespace Lua
