// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

struct lua_State;
class UniqueFileDescriptor;

namespace Lua {

void
InitXattrTable(lua_State *L) noexcept;

void
NewXattrTable(struct lua_State *L, UniqueFileDescriptor fd) noexcept;

} // namespace Lua
