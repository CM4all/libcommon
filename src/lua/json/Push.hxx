// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <boost/json/fwd.hpp>

struct lua_State;

namespace Lua {

void
Push(lua_State *L, const boost::json::string &s);

void
Push(lua_State *L, const boost::json::array &a);

void
Push(lua_State *L, const boost::json::object &o);

void
Push(lua_State *L, const boost::json::value &value);

} // namespace Lua
