// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <cstddef>
#include <span>

class UniqueFileDescriptor;

UniqueFileDescriptor
CreateMemFD(const char *name, unsigned flags=0);

/**
 * Create a memfd with the specified contents.
 */
UniqueFileDescriptor
CreateMemFD(const char *name, std::span<const std::byte> contents);
