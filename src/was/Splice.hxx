// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <cstdint>

struct was_simple;
class FileDescriptor;

bool
SpliceFromWas(was_simple *w, FileDescriptor out_fd) noexcept;

bool
SpliceToWas(was_simple *w, FileDescriptor in_fd, uint_least64_t size) noexcept;
