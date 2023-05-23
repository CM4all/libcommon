// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <cstdint>

struct was_simple;
class FileDescriptor;

bool
SpliceToWas(was_simple *w, FileDescriptor in_fd, uint64_t size) noexcept;
