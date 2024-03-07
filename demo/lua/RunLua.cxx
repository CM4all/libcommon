// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "lua/RunFile.hxx"
#include "lua/json/ToJson.hxx"
#include "lua/mariadb/Init.hxx"
#include "lua/sodium/Init.hxx"
#include "util/PrintException.hxx"
#include "util/ScopeExit.hxx"

extern "C" {
#include <lauxlib.h>
#include <lualib.h>
}

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
try {
	if (argc != 2)
		throw "Usage: RunLua FILE.lua";

	const char *path = argv[1];

	const auto L = luaL_newstate();
	AtScopeExit(L) { lua_close(L); };

	luaL_openlibs(L);
	Lua::InitToJson(L);
	Lua::InitSodium(L);
	Lua::MariaDB::Init(L);

	Lua::RunFile(L, path);
	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
