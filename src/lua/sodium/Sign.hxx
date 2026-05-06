// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

struct lua_State;

namespace Lua {

int
crypto_sign_keypair(lua_State *L);

int
crypto_sign_detached(lua_State *L);

int
crypto_sign_verify_detached(lua_State *L);

} // namespace Lua
