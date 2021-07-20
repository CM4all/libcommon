/*
 * Copyright 2015-2021 Max Kellermann <max.kellermann@gmail.com>
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

#ifndef LUA_PUSH_CLOSURE_HXX
#define LUA_PUSH_CLOSURE_HXX

#include "Assert.hxx"
#include "util/Compiler.h"

extern "C" {
#include <lua.h>
}

#include <utility>
#include <tuple>

namespace Lua {

template<typename... T>
struct CClosure {
	lua_CFunction fn;

	std::tuple<T...> values;
};

template<typename... T>
CClosure<T...>
MakeCClosure(lua_CFunction fn, T&&... values)
{
	return {fn, std::make_tuple(std::forward<T>(values)...)};
}

template<typename... T>
gcc_nonnull_all
void
Push(lua_State *L, const std::tuple<T...> &t)
{
	const ScopeCheckStack check_stack(L, sizeof...(T));

	std::apply([L](const T&... args){
		(Push(L, args), ...);
	}, t);
}

template<typename... T>
gcc_nonnull_all
void
Push(lua_State *L, const CClosure<T...> &value) noexcept
{
	Push(L, value.values);
	lua_pushcclosure(L, value.fn, sizeof...(T));
}

} // namespace Lua

#endif
