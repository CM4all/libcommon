// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <memory>

struct lua_State;

namespace Lua {

struct StateDeleter {
	void operator()(lua_State *state) const noexcept;
};

using State = std::unique_ptr<lua_State, StateDeleter>;

}
