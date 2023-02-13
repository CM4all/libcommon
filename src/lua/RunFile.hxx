// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef LUA_RUN_FILE_HXX
#define LUA_RUN_FILE_HXX

struct lua_State;

namespace Lua {

/**
 * Load, compile and run the specified file.
 *
 * Throws std::runtime_error on error.
 */
void
RunFile(lua_State *L, const char *path);

}

#endif
