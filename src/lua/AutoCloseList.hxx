// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Assert.hxx"
#include "Value.hxx"

namespace Lua {

class AutoCloseList {
	const Value table;

public:
	explicit AutoCloseList(lua_State *L);
	~AutoCloseList() noexcept;

	/**
	 * Schedule a Lua object to be auto-closed by this object's
	 * destructor.
	 */
	template<typename T>
	void Add(lua_State *L, T &&object) {
		const ScopeCheckStack check_stack{L};

		table.Push(L);
		StackPushed(object);

		SetTable(L, RelativeStackIndex{-1},
			 std::forward<T>(object), lua_Integer{1});

		lua_pop(L, 1);
	}
};

} // namespace Lua
