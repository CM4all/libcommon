// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <string_view>

struct lua_State;
class UniqueFileDescriptor;

namespace Lua {

class AutoCloseList;

void
RegisterCgroupInfo(lua_State *L);

void
NewCgroupInfo(lua_State *L, Lua::AutoCloseList &auto_close,
	      std::string_view path) noexcept;

/**
 * @param cgroup_fd an O_RDONLY|O_DIRECTORY file descriptor on the
 * cgroup directory
 */
void
NewCgroupInfo(lua_State *L, Lua::AutoCloseList &auto_close,
	      std::string_view path, UniqueFileDescriptor directory_fd) noexcept;

} // namespace Lua
