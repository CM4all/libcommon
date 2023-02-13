// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#ifndef LARGE_ALLOCATION_HXX
#define LARGE_ALLOCATION_HXX

#include <cstddef>
#include <utility>

/**
 * Allocates anonymous memory using mmap().
 */
class LargeAllocation {
	void *data = nullptr;
	std::size_t the_size;

public:
	LargeAllocation() = default;

	/**
	 * Throws std::bad_alloc on error.
	 */
	explicit LargeAllocation(std::size_t _size);

	LargeAllocation(LargeAllocation &&src) noexcept
		:data(std::exchange(src.data, nullptr)), the_size(src.the_size) {}

	~LargeAllocation() noexcept {
		if (data != nullptr)
			Free(data, the_size);
	}

	LargeAllocation &operator=(LargeAllocation &&src) noexcept {
		using std::swap;
		swap(data, src.data);
		swap(the_size, src.the_size);
		return *this;
	}

	operator bool() const noexcept {
		return data != nullptr;
	}

	void reset() noexcept {
		if (data != nullptr) {
			Free(data, the_size);
			data = nullptr;
		}
	}

	void *get() const noexcept {
		return data;
	}

	std::size_t size() const noexcept {
		return the_size;
	}

private:
	static void Free(void *p, std::size_t size) noexcept;
};

#endif
