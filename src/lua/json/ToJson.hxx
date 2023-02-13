// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <boost/json/fwd.hpp>

struct lua_State;

namespace Lua {

void
InitToJson(lua_State *L) noexcept;

[[gnu::pure]]
boost::json::value
ToJson(lua_State *L, int idx) noexcept;

} // namespace Lua
