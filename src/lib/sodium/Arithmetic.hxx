// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <sodium/utils.h>

#include <cstddef> // for std::byte
#include <span>

inline void
sodium_increment(std::span<std::byte> n) noexcept
{
	sodium_increment(reinterpret_cast<unsigned char *>(n.data()), n.size());
}
