/*
 * Copyright 2007-2018 Content Management AG
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
