// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef LUA_STATE_HXX
#define LUA_STATE_HXX

#include <memory>

struct lua_State;

namespace Lua {

struct StateDeleter {
	void operator()(lua_State *state) const noexcept;
};

using State = std::unique_ptr<lua_State, StateDeleter>;

}

#endif
