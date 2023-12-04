// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "AutoCloseList.hxx"
#include "Close.hxx"
#include "ForEach.hxx"
#include "PushLambda.hxx"
#include "Table.hxx"

namespace Lua {

AutoCloseList::AutoCloseList(lua_State *L)
	:table(L, Lambda([L]{ NewWeakKeyTable(L); })) {}

AutoCloseList::~AutoCloseList() noexcept
{
	const auto L = table.GetState();
	const ScopeCheckStack check_stack{L};

	table.Push(L);

	ForEach(L, RelativeStackIndex{-1}, [L](auto key_idx, auto){
		Close(L, key_idx);
	});

	lua_pop(L, 1);
}

} // namespace Lua
