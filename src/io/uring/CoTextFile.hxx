// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "co/Task.hxx"

#include <string>

#include <sys/stat.h>

class FileDescriptor;

namespace Uring {

class Queue;

Co::Task<std::string>
CoReadTextFile(Queue &queue, FileDescriptor directory_fd, const char *path,
	       size_t max_size=65536);

} // namespace Uring
