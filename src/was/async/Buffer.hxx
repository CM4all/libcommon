/*
 * Copyright 2007-2022 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "util/DisposableBuffer.hxx"

#include <array>
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
