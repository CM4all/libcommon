// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "util/AllocatedArray.hxx"

#include <cstddef>
#include <cassert>

class FileDescriptor;

/**
 * A netstring input buffer.
 */
class NetstringInput {
#ifndef NDEBUG
	bool finished = false;
#endif

	/**
	 * A small buffer with enough space for the header.  What
         * remains in this buffer after the header (i.e. after the
         * colon) will be copied to #value.
	 */
	char header_buffer[32];

	/**
	 * How many chars have been received into #header_buffer
         * already?
	 */
	size_t header_position = 0;

	AllocatedArray<std::byte> value;

	/**
	 * How many bytes have been received into #value already?
	 */
	size_t value_position;

	const size_t max_size;

public:
	explicit NetstringInput(size_t _max_size) noexcept
		:max_size(_max_size) {}

	enum class Result {
		MORE,
		CLOSED,
		FINISHED,
	};

	/**
	 * Throws std::runtime_error on error.
	 */
	Result Receive(FileDescriptor fd);

	AllocatedArray<std::byte> &GetValue() noexcept {
		assert(finished);

		return value;
	}

private:
	bool IsReceivingHeader() const noexcept {
		return value == nullptr;
	}

	Result ReceiveHeader(FileDescriptor fd);
	Result ValueData(size_t nbytes);
	Result ReceiveValue(FileDescriptor fd);
};
