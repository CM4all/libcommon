// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <exception>
#include <string_view>

class FileDescriptor;

void
WriteErrorPipe(FileDescriptor p, std::string_view prefix,
	       std::string_view msg) noexcept;

/**
 * Write a C++ exception into the pipe.
 */
void
WriteErrorPipe(FileDescriptor p, std::string_view prefix,
	       std::exception_ptr e) noexcept;

/**
 * Read an error message from the pipe and if there is one, throw it
 * as a C++ exception.
 */
void
ReadErrorPipe(FileDescriptor p);
