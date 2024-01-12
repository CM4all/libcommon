// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <nlohmann/json_fwd.hpp>

struct lua_State;

namespace Lua {

void
Push(lua_State *L, const nlohmann::json &j);

} // namespace Lua
