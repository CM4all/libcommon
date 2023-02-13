// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "WriteBuffer.hxx"

#include <array>
#include <cstddef>
#include <cassert>

class FileDescriptor;

class MultiWriteBuffer {
	static constexpr size_t MAX_BUFFERS = 32;

	unsigned i = 0, n = 0;

	std::array<WriteBuffer, MAX_BUFFERS> buffers;

public:
	typedef WriteBuffer::Result Result;

	void Push(const void *buffer, size_t size) noexcept {
		assert(n < buffers.size());

		buffers[n++] = WriteBuffer(buffer, size);
	}

	/**
	 * Throws std::system_error on error.
	 */
	Result Write(FileDescriptor fd);
};
