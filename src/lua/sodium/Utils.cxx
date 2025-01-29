// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Utils.hxx"
#include "lua/CheckArg.hxx"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include <sodium/utils.h>

#include <memory>

namespace Lua {

int
bin2hex(lua_State *L)
{
	if (lua_gettop(L) > 1)
		return luaL_error(L, "Too many parameters");

	const auto src = CheckStringView(L, 1);

	const std::size_t dest_length = src.size() * 2;
	const std::size_t dest_capacity = dest_length + 1;
	auto dest = std::make_unique_for_overwrite<char[]>(dest_capacity);
	sodium_bin2hex(dest.get(), dest_capacity,
		       reinterpret_cast<const unsigned char *>(src.data()),
		       src.size());

	lua_pushlstring(L, dest.get(), dest_length);
	return 1;
}

int
hex2bin(lua_State *L)
{
	if (lua_gettop(L) > 1)
		return luaL_error(L, "Too many parameters");

	const auto src = CheckStringView(L, 1);

	const std::size_t dest_capacity = src.size() / 2;
	auto dest = std::make_unique_for_overwrite<char[]>(dest_capacity);

	size_t dest_length;
	if (sodium_hex2bin(reinterpret_cast<unsigned char *>(dest.get()), dest_capacity,
			   src.data(), src.size(),
			   nullptr, &dest_length, nullptr) != 0)
		return 0;

	lua_pushlstring(L, dest.get(), dest_length);
	return 1;
}

} // namespace Lua
