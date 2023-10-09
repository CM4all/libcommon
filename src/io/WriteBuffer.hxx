// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <cstddef>
#include <span>

class FileDescriptor;

class WriteBuffer {
	friend class MultiWriteBuffer;

	const std::byte *buffer, *end;

public:
	WriteBuffer() = default;
	WriteBuffer(const void *_buffer, size_t size)
		:buffer((const std::byte *)_buffer), end(buffer + size) {}

	const void *GetData() const {
		return buffer;
	}

	size_t GetSize() const {
		return end - buffer;
	}

	constexpr operator std::span<const std::byte>() noexcept {
		return {buffer, end};
	}

	enum class Result {
		MORE,
		FINISHED,
	};

	/**
	 * Throws std::system_error on error.
	 */
	Result Write(FileDescriptor fd);
};
