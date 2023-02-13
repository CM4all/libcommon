// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <forward_list>
#include <functional>
#include <new>
#include <span>
#include <string_view>

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

	/**
	 * Concatenate all parameters into a newly allocated
	 * std::string_view.
	 */
	template<typename... Args>
	std::string_view ConcatView(Args... args) noexcept {
		const size_t length = (ConcatLength(args) + ...);
		char *result = NewArray<char>(length);
		ConcatCopyAll(result, args...);
		return {result, length};
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

	std::string_view Dup(std::string_view src) noexcept {
		if (src.empty()) {
			if (src.data() != nullptr)
				return {"", 0};
			return src;
		}

		auto data = NewArray<char>(src.size());
		std::copy(src.begin(), src.end(), data);
		return {data, src.size()};
	}

	const char *DupZ(std::string_view src) {
		char *p = strndup(src.data(), src.size());
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

	static constexpr size_t ConcatLength(std::span<const std::string_view> s) noexcept {
		size_t length = 0;
		for (const auto &i : s)
			length += i.size();
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

	static char *ConcatCopy(char *p, std::span<const std::string_view> s) noexcept {
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

	template<typename... Args>
	std::string_view ConcatView(Args&&... args) const noexcept {
		return allocator.ConcatView(std::forward<Args>(args)...);
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

	std::span<const std::byte> Dup(std::span<const std::byte> src) const noexcept;

	template<typename T>
	std::span<const T> Dup(std::span<const T> src) const noexcept {
		auto dest = Dup(std::as_bytes(src));
		return {
			(const T *)(const void *)(dest.data()),
			dest.size() / sizeof(T),
		};
	}

	std::string_view Dup(std::string_view src) const noexcept {
		return allocator.Dup(src);
	}

	const char *DupZ(std::string_view src) const noexcept {
		return allocator.DupZ(src);
	}
};
