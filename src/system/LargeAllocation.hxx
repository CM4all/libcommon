// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <cstddef>
#include <span>
#include <utility>

/**
 * Allocates anonymous memory using mmap().
 */
class LargeAllocation {
	std::span<std::byte> ptr;

public:
	LargeAllocation() = default;

	/**
	 * Throws std::bad_alloc on error.
	 */
	explicit LargeAllocation(std::size_t _size);

	LargeAllocation(LargeAllocation &&src) noexcept
		:ptr(std::exchange(src.ptr, std::span<std::byte>{})) {}

	~LargeAllocation() noexcept {
		if (ptr.data() != nullptr)
			Free(ptr);
	}

	LargeAllocation &operator=(LargeAllocation &&src) noexcept {
		using std::swap;
		swap(ptr, src.ptr);
		return *this;
	}

	operator bool() const noexcept {
		return ptr.data() != nullptr;
	}

	void reset() noexcept {
		if (ptr.data() != nullptr) {
			Free(ptr);
			ptr = {};
		}
	}

	operator std::span<std::byte>() const noexcept {
		return ptr;
	}

	std::span<std::byte> get() const noexcept {
		return ptr;
	}

private:
	static void Free(std::span<std::byte> p) noexcept;
};
