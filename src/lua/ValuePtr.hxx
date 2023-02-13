// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef LUA_VALUE_PTR_HXX
#define LUA_VALUE_PTR_HXX

#include "Value.hxx"

#include <memory>

namespace Lua {

typedef std::shared_ptr<Lua::Value> ValuePtr;

}

#endif
