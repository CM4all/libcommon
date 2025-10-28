// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

/*
 * Small utilities for PostgreSQL clients.
 */

#pragma once

#include <cstddef>
#include <span>
#include <string_view>

template<typename T> class AllocatedArray;

namespace Pg {

/**
 * Decode a string in the PostgreSQL hex format to a dynamically
 * allocated byte buffer.
 *
 * Throws std::invalid_argument on syntax error.
 */
AllocatedArray<std::byte>
DecodeHex(std::string_view src);

} /* namespace Pg */
