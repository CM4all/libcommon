// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

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
