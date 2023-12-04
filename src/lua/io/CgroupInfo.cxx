// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "CgroupInfo.hxx"
#include "XattrTable.hxx"
#include "lua/AutoCloseList.hxx"
#include "lua/Class.hxx"
#include "lua/Error.hxx"
#include "lua/FenvCache.hxx"
#include "io/Beneath.hxx"
#include "io/FileAt.hxx"
#include "io/Open.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "util/StringAPI.hxx"

extern "C" {
#include <lauxlib.h>
}

namespace Lua {

class CgroupInfo {
	Lua::AutoCloseList *auto_close;

	const std::string path;

public:
	CgroupInfo(lua_State *L, Lua::AutoCloseList &_auto_close,
		std::string_view _path);

	bool IsStale() const noexcept {
		return auto_close == nullptr;
	}

	int Close(lua_State *) {
		auto_close = nullptr;
		return 0;
	}

	int Index(lua_State *L);
};

static constexpr char lua_cgroup_info_class[] = "cgroup_info";
typedef Lua::Class<CgroupInfo, lua_cgroup_info_class> LuaCgroupInfo;

CgroupInfo::CgroupInfo(lua_State *L, Lua::AutoCloseList &_auto_close,
		 std::string_view _path)
	:auto_close(&_auto_close),
	 path(_path)
{
	auto_close->Add(L, Lua::RelativeStackIndex{-1});
}

inline int
CgroupInfo::Index(lua_State *L)
{
	if (lua_gettop(L) != 2)
		return luaL_error(L, "Invalid parameters");

	const char *const name = luaL_checkstring(L, 2);

	if (IsStale())
		return luaL_error(L, "Stale object");

	// look it up in the fenv (our cache)
	if (Lua::GetFenvCache(L, 1, name))
		return 1;

	if (StringIsEqual(name, "path")) {
		Lua::Push(L, path);
		return 1;
	} else if (StringIsEqual(name, "xattr")) {
		try {
			const auto sys_fs_cgroup = OpenPath("/sys/fs/cgroup");
			auto fd = OpenReadOnlyBeneath({sys_fs_cgroup, path.c_str() + 1});
			Lua::NewXattrTable(L, std::move(fd));
		} catch (...) {
			Lua::RaiseCurrent(L);
		}

		// auto-close the file descriptor when the connection is closed
		auto_close->Add(L, Lua::RelativeStackIndex{-1});

		// copy a reference to the fenv (our cache)
		Lua::SetFenvCache(L, 1, name, Lua::RelativeStackIndex{-1});

		return 1;
	} else
		return luaL_error(L, "Unknown attribute");
}

void
RegisterCgroupInfo(lua_State *L)
{
	using namespace Lua;

	LuaCgroupInfo::Register(L);
	SetTable(L, RelativeStackIndex{-1}, "__close", LuaCgroupInfo::WrapMethod<&CgroupInfo::Close>());
	SetField(L, RelativeStackIndex{-1}, "__index", LuaCgroupInfo::WrapMethod<&CgroupInfo::Index>());
	lua_pop(L, 1);
}

void
NewCgroupInfo(lua_State *L, Lua::AutoCloseList &auto_close,
	      std::string_view path) noexcept
{
	LuaCgroupInfo::New(L, L, auto_close, path);
}

} // namespace Lua
