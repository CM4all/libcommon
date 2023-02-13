// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "LargeAllocation.hxx"

/**
 * A unique pointer to a variable-sized object allocated with
 * #LargeAllocation.
 */
template<typename T>
class LargeObject {
	LargeAllocation allocation;

public:
	LargeObject() = default;

	/**
	 * Allocate and construct a new instance.
	 *
	 * Throws std::bad_alloc on error.
	 *
	 * @param _size the number of bytes to allocate; must be at
	 * least as large as sizeof(T)
	 * @param args arguments passed to the T constructor
	 */
	template<typename... Args>
	explicit LargeObject(size_t _size, Args&&... args)
		:allocation(_size) {
		new(allocation.get()) T(std::forward<Args>(args)...);
	}

	LargeObject(LargeObject &&src) = default;

	~LargeObject() noexcept {
		if (get() != nullptr)
			get()->~T();
	}

	LargeObject &operator=(LargeObject &&src) = default;

	operator bool() const noexcept {
		return allocation;
	}

	/**
	 * Returns the allocated size, i.e. the size passed to the
	 * constructor.
	 */
	size_t size() const noexcept {
		return allocation.size();
	}

	void reset() noexcept {
		if (get() != nullptr) {
			get()->~T();
			allocation.reset();
		}
	}

	T *get() const noexcept {
		return static_cast<T *>(allocation.get());
	}

	T *operator->() const noexcept {
		return get();
	}

	T &operator*() const noexcept {
		return *get();
	}
};
