/*
 * Copyright 2020 CM4all GmbH
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

#include "ShallowCopy.hxx"

#include <iterator>
#include <type_traits>
#include <utility>

struct IntrusiveForwardListHook {
	IntrusiveForwardListHook *next = nullptr;
};

template<typename T>
class IntrusiveForwardList {
	IntrusiveForwardListHook head;

	static constexpr T *Cast(IntrusiveForwardListHook *hook) noexcept {
		static_assert(std::is_base_of<IntrusiveForwardListHook, T>::value);
		return static_cast<T *>(hook);
	}

	static constexpr const T *Cast(const IntrusiveForwardListHook *hook) noexcept {
		static_assert(std::is_base_of<IntrusiveForwardListHook, T>::value);
		return static_cast<const T *>(hook);
	}

public:
	IntrusiveForwardList() = default;

	IntrusiveForwardList(IntrusiveForwardList &&src) noexcept
		:head{std::exchange(src.head.next, nullptr)} {}

	constexpr IntrusiveForwardList(ShallowCopy, const IntrusiveForwardList &src) noexcept
		:head(src.head) {}

	IntrusiveForwardList &operator=(IntrusiveForwardList &&src) noexcept {
		using std::swap;
		swap(head, src.head);
		return *this;
	}

	constexpr bool empty() const noexcept {
		return head.next == nullptr;
	}

	void clear() noexcept {
		head = {};
	}

	template<typename D>
	void clear_and_dispose(D &&disposer) noexcept {
		while (!empty()) {
			auto *item = &front();
			pop_front();
			disposer(item);
		}
	}

	const T &front() const noexcept {
		return *Cast(head.next);
	}

	T &front() noexcept {
		return *Cast(head.next);
	}

	void pop_front() noexcept {
		head.next = head.next->next;
	}

	class const_iterator;

	class iterator final
		: public std::iterator<std::forward_iterator_tag, T> {

		friend IntrusiveForwardList;
		friend const_iterator;

		IntrusiveForwardListHook *cursor;

		constexpr iterator(IntrusiveForwardListHook *_cursor) noexcept
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

	constexpr iterator before_begin() noexcept {
		return {&head};
	}

	constexpr iterator begin() noexcept {
		return {head.next};
	}

	static constexpr iterator end() noexcept {
		return {nullptr};
	}

	class const_iterator final
		: public std::iterator<std::forward_iterator_tag, const T> {

		friend IntrusiveForwardList;

		const IntrusiveForwardListHook *cursor;

		constexpr const_iterator(const IntrusiveForwardListHook *_cursor) noexcept
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

	void push_front(T &t) noexcept {
		t.next = head.next;
		head.next = &t;
	}

	static iterator insert_after(iterator pos, T &t) noexcept {
		t.next = pos->next;
		pos->next = &t;
		return &t;
	}

	void erase_after(iterator pos) noexcept {
		pos->next = pos->next->next;
	}

	void reverse() noexcept {
		if (empty())
			return;

		/* the first item will be the last, and will stay
		   there; during the loop, it will divide the list
		   between "old order" (right of it) and "new
		   (reversed) order" (left of it) */
		const auto middle = begin();

		while (std::next(middle) != end()) {
			/* remove the item after the "middle", and
			   move it to the front */
			auto i = std::next(middle);
			erase_after(middle);
			push_front(*i);
		}
	}
};
