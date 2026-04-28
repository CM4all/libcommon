// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Error.hxx"
#include "util/Exception.hxx"

extern "C" {
#include <lua.h>
}

#include <cassert>
#include <utility> // for std::unreachable()

namespace Lua {

/**
 * Attempt to extract an error string from the stack; if the stack
 * item cannot be converted to a string, return the Lua type name
 * instead.
 */
[[gnu::pure]]
static const char *
GetErrorMessage(lua_State *L, int idx) noexcept
{
	if (const char *const s = lua_tostring(L, idx); s != nullptr)
		return s;

	return lua_typename(L, lua_type(L, idx));
}

Error::Error(lua_State *L, int idx) noexcept
	:Error(GetErrorMessage(L, idx))
{
}

Error
PopError(lua_State *L)
{
	Error e{L};
	lua_pop(L, 1);
	return e;
}

void
Push(lua_State *L, std::exception_ptr e) noexcept
{
	assert(e);

	lua_pushstring(L, GetFullMessage(std::move(e)).c_str());
}

void
Raise(lua_State *L, std::exception_ptr e)
{
	Push(L, std::move(e));
	lua_error(L);

	/* this is unreachable because lua_error() never returns, but
	   the C header doesn't declare it that way */
	std::unreachable();
}

void
RaiseCurrent(lua_State *L)
{
	auto e = std::current_exception();
	if (e)
		Raise(L, std::move(e));
	else
		throw;
}

} // namespace Lua
