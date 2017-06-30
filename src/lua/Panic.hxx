/*
 * Copyright (C) 2017 Max Kellermann <max.kellermann@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef LUA_PANIC_HXX
#define LUA_PANIC_HXX

#include "Error.hxx"

#include "util/Compiler.h"

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
	gcc_noreturn
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
