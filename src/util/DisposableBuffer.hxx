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

#include "DisposablePointer.hxx"

#include <span>
#include <string_view>

/**
 * A #DisposablePointer wrapper which knows its size.  For convenience, it can
 * be casted implicitly to various classes such as std::string_view.
 */
class DisposableBuffer {
	DisposablePointer data_;
	std::size_t size_ = 0;

public:
	DisposableBuffer() = default;
	DisposableBuffer(std::nullptr_t) noexcept {}

	template<typename D>
	DisposableBuffer(D &&_data, std::size_t _size) noexcept
		:data_(std::forward<D>(_data)), size_(_size) {}

	DisposableBuffer(DisposableBuffer &&) noexcept = default;
	DisposableBuffer &operator=(DisposableBuffer &&) noexcept = default;

	static DisposableBuffer Dup(std::string_view src) noexcept;
	static DisposableBuffer Dup(std::span<const std::byte> src) noexcept;

	operator bool() const noexcept {
		return data_;
	}

	void *data() const noexcept {
		return data_.get();
	}

	std::size_t size() const noexcept {
		return size_;
	}

	bool empty() const noexcept {
		return size() == 0;
	}

	operator std::string_view() const noexcept {
		return {(const char *)data_.get(), size()};
	}

	operator std::span<const std::byte>() const noexcept {
		return {(const std::byte *)data_.get(), size()};
	}
};
