// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <stdint.h>

namespace Net {
namespace Log {

enum class Type : uint8_t;

[[gnu::const]]
const char *
ToString(Type type) noexcept;

/**
 * Throws std::invalid_argument on error.
 */
Type
ParseType(const char *s);

}}
