// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "util/AllocatedArray.hxx"

#include <cstddef>
#include <cassert>

class FileDescriptor;

/**
 * A netstring input buffer.
 */
class NetstringInput {
	enum class State {
		HEADER,
		VALUE,
	};

	State state = State::HEADER;

#ifndef NDEBUG
	bool finished = false;
#endif

	char header_buffer[32];
	size_t header_position = 0;

	AllocatedArray<std::byte> value;
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
	Result ReceiveHeader(FileDescriptor fd);
	Result ValueData(size_t nbytes);
	Result ReceiveValue(FileDescriptor fd);
};
