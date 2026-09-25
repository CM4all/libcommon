// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <nlohmann/json_fwd.hpp>

struct lua_State;

namespace Lua {

/**
 * Throws on error (e.g. the table nesting is too deep).
 */
nlohmann::json
ToJson(lua_State *L, int idx);

} // namespace Lua
