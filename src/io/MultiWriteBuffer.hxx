// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "WriteBuffer.hxx"

#include <array>
#include <cstddef>
#include <cassert>
#include <span>

class FileDescriptor;

class MultiWriteBuffer {
	static constexpr size_t MAX_BUFFERS = 32;

	unsigned i = 0, n = 0;

	std::array<WriteBuffer, MAX_BUFFERS> buffers;

public:
	typedef WriteBuffer::Result Result;

	void Push(std::span<const std::byte> s) noexcept {
		assert(n < buffers.size());

		buffers[n++] = WriteBuffer(s.data(), s.size());
	}

	/**
	 * Throws std::system_error on error.
	 */
	Result Write(FileDescriptor fd);
};
