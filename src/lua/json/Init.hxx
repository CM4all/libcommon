// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

struct lua_State;

namespace Lua {

void
InitJson(lua_State *L) noexcept;

} // namespace Lua
