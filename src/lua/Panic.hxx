// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef LUA_PANIC_HXX
#define LUA_PANIC_HXX

#include "Error.hxx"

extern "C" {
#include <lua.h>
}

#include <setjmp.h>

namespace Lua {

/**
 * Installs a Lua panic handler using lua_atpanic() which converts the
 * "panic" condition to a C++ exception.  Before doing anything that
 * could raise a panic, do:
 *
 *    if (setjmp(j) != 0)
 *      throw PopError(L);
 */
class ScopePanicHandler {
	lua_State *const L;
	const lua_CFunction old;

public:
	jmp_buf j;

	explicit ScopePanicHandler(lua_State *_L);
	~ScopePanicHandler();

	ScopePanicHandler(const ScopePanicHandler &) = delete;
	ScopePanicHandler &operator=(const ScopePanicHandler &) = delete;

private:
	[[noreturn]]
	static int PanicHandler(lua_State *L);
};

/**
 * Call a function, and if a Lua panic occurs during that, convert it
 * to a C++ exception and throw it.
 */
template<typename F>
auto
WithPanicHandler(lua_State *L, F &&f)
{
	Lua::ScopePanicHandler panic(L);
	if (setjmp(panic.j) != 0)
		throw PopError(L);

	return f();
}

}

#endif
