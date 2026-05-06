// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include <cstddef>
#include <span>

namespace Lua {

/**
 * Check whether the specified Lua stack argument is a libsodium key of the specified type.
 *
 * @param T a fixed-size std::span<const std::byte> (e.g. #CryptoBoxSecretKeyView)
 * @param arg index on the Lua stack
 * @param msg error message if the string does not have the expected size
 */
template<typename T>
requires(T::extent != std::dynamic_extent)
inline T
CheckSodiumKey(lua_State *L, int arg, const char *msg)
{
	std::size_t size;
	const char *data = luaL_checklstring(L, arg, &size);

	luaL_argcheck(L, size == T::extent, arg, msg);

	return std::span<const std::byte>(reinterpret_cast<const std::byte *>(data), size).first<T::extent>();
}

} // namespace Lua
