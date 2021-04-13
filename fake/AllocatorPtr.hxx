/*
 * Copyright 2017-2021 CM4all GmbH
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

#ifndef ALLOCATOR_PTR_HXX
#define ALLOCATOR_PTR_HXX

#include "util/ConstBuffer.hxx"
#include "util/StringView.hxx"

#include <forward_list>
#include <functional>
#include <new>

#include <stdlib.h>
#include <string.h>

class Allocator {
	std::forward_list<std::function<void()>> cleanup;

public:
	Allocator() = default;
	Allocator(Allocator &&src) = default;

	~Allocator() noexcept {
		for (auto &i : cleanup)
			i();
	}

	Allocator &operator=(Allocator &&src) = delete;

	void *Allocate(size_t size) {
		void *p = malloc(size);
		if (p == nullptr)
			throw std::bad_alloc();

		cleanup.emplace_front([p](){ free(p); });
		return p;
	}

	char *Dup(const char *src) {
		char *p = strdup(src);
		if (p == nullptr)
			throw std::bad_alloc();

		cleanup.emplace_front([p](){ free(p); });
		return p;
	}

	const char *CheckDup(const char *src) noexcept {
		return src != nullptr ? Dup(src) : nullptr;
	}

	template<typename... Args>
	char *Concat(Args... args) noexcept {
		const size_t length = (ConcatLength(args) + ...);
		char *result = NewArray<char>(length + 1);
		*ConcatCopyAll(result, args...) = 0;
		return result;
	}

	template<typename T, typename... Args>
	T *New(Args&&... args) noexcept {
		auto p = new T(std::forward<Args>(args)...);
		cleanup.emplace_front([p](){ delete p; });
		return p;
	}

	template<typename T>
	T *NewArray(size_t n) noexcept {
		auto p = new T[n];
		cleanup.emplace_front([p](){ delete[] p; });
		return p;
	}

	StringView Dup(StringView src) noexcept {
		if (src.empty()) {
			if (src != nullptr)
				src.data = "";
			return src;
		}

		auto data = NewArray<char>(src.size);
		std::copy_n(src.data, src.size, data);
		return {data, src.size};
	}

	const char *DupZ(StringView src) {
		char *p = strndup(src.data, src.size);
		if (p == nullptr)
			throw std::bad_alloc();

		cleanup.emplace_front([p](){ free(p); });
		return p;
	}

private:
	static constexpr size_t ConcatLength(char) noexcept {
		return 1;
	}

	static constexpr size_t ConcatLength(std::string_view sv) noexcept {
		return sv.size();
	}

	static constexpr size_t ConcatLength(ConstBuffer<StringView> s) noexcept {
		size_t length = 0;
		for (const auto &i : s)
			length += i.size;
		return length;
	}

	static char *ConcatCopy(char *p, const char *s) noexcept {
		return stpcpy(p, s);
	}

	static char *ConcatCopy(char *p, char ch) noexcept {
		*p++ = ch;
		return p;
	}

	static char *ConcatCopy(char *p, std::string_view sv) noexcept {
		return (char *)mempcpy(p, sv.data(), sv.size());
	}

	static char *ConcatCopy(char *p, ConstBuffer<StringView> s) noexcept {
		for (const auto &i : s)
			p = ConcatCopy(p, i);
		return p;
	}

	template<typename... Args>
	static char *ConcatCopyAll(char *p, Args... args) noexcept {
		((p = ConcatCopy(p, args)), ...);
		return p;
	}
};

class AllocatorPtr {
	Allocator &allocator;

public:
	constexpr AllocatorPtr(Allocator &_allocator) noexcept
		:allocator(_allocator) {}

	const char *Dup(const char *src) const noexcept {
		return allocator.Dup(src);
	}

	const char *CheckDup(const char *src) const noexcept {
		return allocator.CheckDup(src);
	}

	template<typename... Args>
	char *Concat(Args&&... args) const noexcept {
		return allocator.Concat(std::forward<Args>(args)...);
	}

	template<typename T, typename... Args>
	T *New(Args&&... args) const noexcept {
		return allocator.New<T>(std::forward<Args>(args)...);
	}

	template<typename T>
	T *NewArray(size_t n) const noexcept {
		return allocator.NewArray<T>(n);
	}

	void *Dup(const void *data, size_t size) const noexcept {
		auto p = allocator.Allocate(size);
		memcpy(p, data, size);
		return p;
	}

	ConstBuffer<void> Dup(ConstBuffer<void> src) const noexcept;

	template<typename T>
	ConstBuffer<T> Dup(ConstBuffer<T> src) const noexcept {
		return ConstBuffer<T>::FromVoid(Dup(src.ToVoid()));
	}

	StringView Dup(StringView src) const noexcept {
		return allocator.Dup(src);
	}

	const char *DupZ(StringView src) const noexcept {
		return allocator.DupZ(src);
	}
};

#endif
