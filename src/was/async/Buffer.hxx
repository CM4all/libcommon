// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "util/DisposableBuffer.hxx"

#include <array>
#include <optional>
#include <span>

namespace Was {

class Buffer {
	static constexpr std::size_t MAX_SIZE = 256 * 1024;
	static constexpr std::size_t UNKNOWN_SIZE = ~std::size_t{};

	std::size_t length = UNKNOWN_SIZE;
	std::size_t fill = 0;

	using Array = std::array<std::byte, MAX_SIZE>;
	Array buffer;

public:
	static constexpr std::size_t max_size() noexcept {
		return MAX_SIZE;
	}

	std::size_t GetFill() const noexcept {
		return fill;
	}

	/**
	 * Returns the total length announced by #WAS_COMMAND_LENGTH, or
	 * std::nullopt if it has not been announced yet.
	 */
	std::optional<std::size_t> GetLength() const noexcept {
		if (length == UNKNOWN_SIZE)
			return std::nullopt;

		return length;
	}

	bool SetLength(std::size_t _length) noexcept {
		if (length != UNKNOWN_SIZE || _length > MAX_SIZE)
			return false;

		length = _length;
		return true;
	}

	bool IsComplete() const noexcept {
		return fill == length;
	}

	constexpr std::span<std::byte> Write() noexcept {
		return {&buffer[fill], MAX_SIZE - fill};
	}

	void Append(std::size_t nbytes) noexcept {
		fill += nbytes;
	}

	static void Dispose(void *ptr) noexcept {
		auto &buffer = *(Array *)ptr;
		auto &ib = ContainerCast(buffer, &Buffer::buffer);
		delete &ib;
	}

	DisposablePointer ToDisposablePointer() noexcept {
		return {&buffer, Dispose};
	}

	DisposableBuffer ToDisposableBuffer() noexcept {
		return {ToDisposablePointer(), fill};
	}
};

} // namespace Was
