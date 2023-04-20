// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

struct lua_State;

namespace Lua {

int
crypto_box_keypair(lua_State *L);

int
crypto_box_seal(lua_State *L);

int
crypto_box_seal_open(lua_State *L);

} // namespace Lua
