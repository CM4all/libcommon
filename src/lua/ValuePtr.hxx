// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "Value.hxx"

#include <memory>

namespace Lua {

typedef std::shared_ptr<Lua::Value> ValuePtr;

}
