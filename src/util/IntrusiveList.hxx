/*
 * Copyright 2020 Max Kellermann <max.kellermann@gmail.com>
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

#include <iterator>
#include <type_traits>
#include <utility>

struct IntrusiveListHook {
	IntrusiveListHook *next, *prev;

	void unlink() noexcept {
		next->prev = prev;
		prev->next = next;
	}
};

template<typename T>
class IntrusiveList {
	IntrusiveListHook head{&head, &head};

	static constexpr T *Cast(IntrusiveListHook *hook) noexcept {
		static_assert(std::is_base_of<IntrusiveListHook, T>::value);
		return static_cast<T *>(hook);
	}

	static constexpr const T *Cast(const IntrusiveListHook *hook) noexcept {
		static_assert(std::is_base_of<IntrusiveListHook, T>::value);
		return static_cast<const T *>(hook);
	}

public:
	IntrusiveList() = default;

	IntrusiveList(IntrusiveList &&src) noexcept {
		if (src.empty())
			return;

		head = src.head;
		head.next->prev = &head;
		head.prev->next = &head;

		src.head.next = &src.head;
		src.head.prev = &src.head;
	}

	IntrusiveList &operator=(IntrusiveList &&) = delete;

	constexpr bool empty() const noexcept {
		return head.next == &head;
	}

	void clear() noexcept {
		head = {&head, &head};
	}

	template<typename D>
	void clear_and_dispose(D &&disposer) noexcept {
		while (!empty()) {
			auto *item = &front();
			pop_front();
			disposer(item);
		}
	}

	template<typename P, typename D>
	void remove_and_dispose_if(P &&pred, D &&dispose) noexcept {
		auto *n = head.next;

		while (n != &head) {
			auto *i = Cast(n);
			n = n->next;

			if (pred(*i)) {
				i->unlink();
				dispose(i);
			}
		}
	}

	const T &front() const noexcept {
		return *Cast(head.next);
	}

	T &front() noexcept {
		return *Cast(head.next);
	}

	void pop_front() noexcept {
		front().unlink();
	}

	T &back() noexcept {
		return *Cast(head.prev);
	}

	void pop_back() noexcept {
		back().unlink();
	}

	class const_iterator;

	class iterator final
		: public std::iterator<std::forward_iterator_tag, T> {

		friend IntrusiveList;
		friend const_iterator;

		IntrusiveListHook *cursor;

		constexpr iterator(IntrusiveListHook *_cursor) noexcept
			:cursor(_cursor) {}

	public:
		iterator() = default;

		constexpr bool operator==(const iterator &other) const noexcept {
			return cursor == other.cursor;
		}

		constexpr bool operator!=(const iterator &other) const noexcept {
			return !(*this == other);
		}

		constexpr T &operator*() const noexcept {
			return *Cast(cursor);
		}

		constexpr T *operator->() const noexcept {
			return Cast(cursor);
		}

		iterator &operator++() noexcept {
			cursor = cursor->next;
			return *this;
		}
	};

	constexpr iterator begin() noexcept {
		return {head.next};
	}

	constexpr iterator end() noexcept {
		return {&head};
	}

	static constexpr iterator iterator_to(T &t) noexcept {
		return {&t};
	}

	class const_iterator final
		: public std::iterator<std::forward_iterator_tag, const T> {

		friend IntrusiveList;

		const IntrusiveListHook *cursor;

		constexpr const_iterator(const IntrusiveListHook *_cursor) noexcept
			:cursor(_cursor) {}

	public:
		const_iterator() = default;

		const_iterator(iterator src) noexcept
			:cursor(src.cursor) {}

		constexpr bool operator==(const const_iterator &other) const noexcept {
			return cursor == other.cursor;
		}

		constexpr bool operator!=(const const_iterator &other) const noexcept {
			return !(*this == other);
		}

		constexpr const T &operator*() const noexcept {
			return *Cast(cursor);
		}

		constexpr const T *operator->() const noexcept {
			return Cast(cursor);
		}

		const_iterator &operator++() noexcept {
			cursor = cursor->next;
			return *this;
		}
	};

	constexpr const_iterator begin() const noexcept {
		return {head.next};
	}

	constexpr const_iterator end() const noexcept {
		return {&head};
	}

	static constexpr iterator iterator_to(const T &t) noexcept {
		return {&t};
	}

	void push_front(T &t) noexcept {
		head.next->prev = &t;
		t.next = head.next;
		head.next = &t;
		t.prev = &head;
	}

	void push_back(T &t) noexcept {
		head.prev->next = &t;
		t.prev = head.prev;
		head.prev = &t;
		t.next = &head;
	}
};
