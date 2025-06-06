// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

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

#include <string>

namespace Lua {

class CgroupInfo {
	Lua::AutoCloseList *auto_close;

	const std::string path;

	UniqueFileDescriptor directory_fd;

public:
	CgroupInfo(lua_State *L, Lua::AutoCloseList &_auto_close,
		   std::string_view _path,
		   UniqueFileDescriptor &&_directory_fd={});

	bool IsStale() const noexcept {
		return auto_close == nullptr;
	}

	int Close(lua_State *) {
		auto_close = nullptr;
		return 0;
	}

	int Index(lua_State *L);

private:
	UniqueFileDescriptor OpenDirectory();
};

static constexpr char lua_cgroup_info_class[] = "cgroup_info";
typedef Lua::Class<CgroupInfo, lua_cgroup_info_class> LuaCgroupInfo;

CgroupInfo::CgroupInfo(lua_State *L, Lua::AutoCloseList &_auto_close,
		       std::string_view _path,
		       UniqueFileDescriptor &&_directory_fd)
	:auto_close(&_auto_close),
	 path(_path),
	 directory_fd(std::move(_directory_fd))
{
	auto_close->Add(L, Lua::RelativeStackIndex{-1});

	lua_newtable(L);
	lua_setfenv(L, -2);
}

inline UniqueFileDescriptor
CgroupInfo::OpenDirectory()
{
	if (directory_fd.IsDefined())
		return std::move(directory_fd);

	const auto sys_fs_cgroup = OpenPath("/sys/fs/cgroup");
	return OpenReadOnlyBeneath({sys_fs_cgroup, path.c_str() + 1});
}

inline int
CgroupInfo::Index(lua_State *L)
{
	if (lua_gettop(L) != 2)
		return luaL_error(L, "Invalid parameters");

	constexpr StackIndex name_idx{2};
	const char *const name = luaL_checkstring(L, 2);

	if (IsStale())
		return luaL_error(L, "Stale object");

	// look it up in the fenv (our cache)
	if (Lua::GetFenvCache(L, 1, name_idx))
		return 1;

	if (StringIsEqual(name, "path")) {
		Lua::Push(L, path);
		return 1;
	} else if (StringIsEqual(name, "xattr")) {
		try {
			Lua::NewXattrTable(L, OpenDirectory());
		} catch (...) {
			Lua::RaiseCurrent(L);
		}

		// auto-close the file descriptor when the connection is closed
		auto_close->Add(L, Lua::RelativeStackIndex{-1});

		// copy a reference to the fenv (our cache)
		Lua::SetFenvCache(L, 1, name_idx, Lua::RelativeStackIndex{-1});

		return 1;
	} else if (StringIsEqual(name, "parent")) {
		const auto slash = path.rfind('/');
		if (slash == path.npos || slash == 0)
			return 0;

		NewCgroupInfo(L, *auto_close,
			      std::string_view{path}.substr(0, slash));

		// copy a reference to the fenv (our cache)
		Lua::SetFenvCache(L, 1, name_idx, Lua::RelativeStackIndex{-1});

		return 1;
	} else
		return 0;
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

void
NewCgroupInfo(lua_State *L, Lua::AutoCloseList &auto_close,
	      std::string_view path, UniqueFileDescriptor directory_fd) noexcept
{
	LuaCgroupInfo::New(L, L, auto_close, path, std::move(directory_fd));
}

} // namespace Lua
