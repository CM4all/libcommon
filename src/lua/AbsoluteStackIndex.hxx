// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "StackIndex.hxx"

extern "C" {
#include <lua.h>
}

namespace Lua {

/**
 * Wrap the specified numeric Lua stack index in a #StackIndex and
 * make sure it is an absolute index (i.e. not negative).
 */
[[gnu::pure]]
inline StackIndex
ToAbsoluteStackIndex(lua_State *L, int idx) noexcept
{
	if (idx < 0)
		idx += lua_gettop(L) + 1;

	return StackIndex{idx};
}

} // namespace Lua
