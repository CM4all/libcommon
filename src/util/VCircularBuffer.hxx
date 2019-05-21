/*
 * Copyright 2017-2018 Content Management AG
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

#include "Cast.hxx"
#include "WritableBuffer.hxx"
#include "MemberIteratorAdapter.hxx"

#include <boost/intrusive/slist.hpp>

#include <new>
#include <iterator>

#include <assert.h>

/**
 * A fixed-size circular buffer which allocates variable-size items.
 *
 * The memory chunk used to store these items is managed by the
 * caller.  This class does not care for allocating or freeing it.
 */
template<typename T>
class VCircularBuffer {
public:
	typedef T value_type;
	typedef T &reference;
	typedef const T &const_reference;
	typedef T *pointer;
	typedef const T *const_pointer;

private:
	struct Item {
		using ListHook =
			boost::intrusive::slist_member_hook<boost::intrusive::link_mode<boost::intrusive::normal_link>>;
		ListHook siblings;

		T value;

		template<typename... Args>
		explicit Item(Args&&... args)
			:value(std::forward<Args>(args)...) {}

		static constexpr const Item &Cast(const_reference v) noexcept {
			return ContainerCast(v, &Item::value);
		}

		static constexpr Item &Cast(reference v) noexcept {
			return ContainerCast(v, &Item::value);
		}

		struct Disposer {
			void operator()(Item *item) noexcept {
				item->~Item();
			}
		};
	};

	using List =
		boost::intrusive::slist<Item,
					boost::intrusive::member_hook<Item,
								      typename Item::ListHook,
								      &Item::siblings>,
					boost::intrusive::cache_last<true>,
					boost::intrusive::constant_time_size<true>>;

	List *const list;
	const size_t buffer_size;

	size_t tail_item_size;

public:
	VCircularBuffer(WritableBuffer<void> buffer) noexcept
		:list(::new(Align(buffer, alignof(List)).data) List()),
		 buffer_size(Align(buffer, alignof(List)).size) {
		assert(buffer.size >= sizeof(List) + sizeof(Item));
	}

	~VCircularBuffer() noexcept {
		clear();
		list->~List();
	}

	constexpr bool empty() const noexcept {
		return list->empty();
	}

	constexpr size_t size() const noexcept {
		return list->size();
	}

	void clear() noexcept {
		list->clear_and_dispose(typename Item::Disposer());
	}

#if !GCC_OLDER_THAN(5,0)
	constexpr
#endif
	reference front() noexcept {
		return list->front().value;
	}

#if !GCC_OLDER_THAN(5,0)
	constexpr
#endif
	reference back() noexcept {
		return list->back().value;
	}

	constexpr const_reference front() const noexcept {
		return list->front().value;
	}

	constexpr const_reference back() const noexcept {
		return list->back().value;
	}

	void pop_front() noexcept {
		assert(!empty());

		list->pop_front_and_dispose(typename Item::Disposer());
	}

	template<typename... Args>
	reference emplace_back(size_t value_size, Args&&... args) {
		const size_t item_size = ValueSizeToItemSize(value_size);
		void *p = MakeFree(item_size);
		auto *item = ::new(p) Item(std::forward<Args>(args)...);
		list->push_back(*item);
		tail_item_size = item_size;
		return item->value;
	}

	/**
	 * Like emplace_back(), but call a check function with the new
	 * item.  That function may throw an exception, which rolls
	 * back the newly constructed item.
	 */
	template<typename C, typename... Args>
	reference check_emplace_back(C &&check,
				     size_t value_size, Args&&... args) {
		const size_t item_size = ValueSizeToItemSize(value_size);
		void *p = MakeFree(item_size);
		auto *item = ::new(p) Item(std::forward<Args>(args)...);

		try {
			check(item->value);
		} catch (...) {
			item->~Item();
			throw;
		}

		list->push_back(*item);
		tail_item_size = item_size;
		return item->value;
	}

	using const_iterator = MemberIteratorAdapter<typename List::const_iterator,
						     const Item, const T,
						     &Item::value>;

	constexpr const_iterator begin() const noexcept {
		return list->cbegin();
	}

	constexpr const_iterator end() const noexcept {
		return list->cend();
	}

	constexpr const_iterator iterator_to(const_reference value) const noexcept {
		return list->iterator_to(Item::Cast(value));
	}

	using iterator = MemberIteratorAdapter<typename List::iterator,
					       Item, T,
					       &Item::value>;

#if !GCC_OLDER_THAN(5,0)
	constexpr
#endif
	iterator begin() noexcept {
		return list->begin();
	}

#if !GCC_OLDER_THAN(5,0)
	constexpr
#endif
	iterator end() noexcept {
		return list->end();
	}

#if !GCC_OLDER_THAN(5,0)
	constexpr
#endif
	iterator iterator_to(reference value) noexcept {
		return list->iterator_to(Item::Cast(value));
	}

private:
	template<typename U>
	static constexpr U DeltaRoundUp(U p, U align) noexcept {
		return (align - (p % align)) % align;
	}

	static constexpr uint8_t *Align(uint8_t *p, size_t align) noexcept {
		return p + DeltaRoundUp((size_t)p, align);
	}

	static constexpr void *Align(void *p, size_t align) noexcept {
		return Align((uint8_t *)p, align);
	}

	static constexpr void *Align(void *p) noexcept {
		return Align(p, alignof(Item));
	}

	static constexpr WritableBuffer<uint8_t> Align(WritableBuffer<uint8_t> src,
						       size_t align) noexcept {
		return WritableBuffer<uint8_t>(std::next(src.begin(),
							 DeltaRoundUp((size_t)src.data, align)),
					       src.end());
	}

	static constexpr WritableBuffer<void> Align(WritableBuffer<void> src,
						    size_t align) noexcept {
		return Align(WritableBuffer<uint8_t>::FromVoid(src),
			     align).ToVoid();
	}

	static constexpr WritableBuffer<void> Align(WritableBuffer<void> src) noexcept {
		return Align(src, alignof(Item));
	}

	static constexpr size_t ValueSizeToItemSize(size_t value_size) noexcept {
		return ContainerAttributeOffset<Item, T>(&Item::value) + value_size;
	}

	static constexpr size_t BytesDelta(const void *a, const void *b) noexcept {
		return static_cast<const uint8_t *>(b)
			- static_cast<const uint8_t *>(a);
	}

	constexpr void *FirstSlot() noexcept {
		return Align(list + 1);
	}

	constexpr void *AtBuffer(size_t offset) noexcept {
		return OffsetPointer(list, offset);
	}

	constexpr void *AfterLast(Item *last) const noexcept {
		return Align(OffsetPointer(last, tail_item_size),
			     alignof(Item));
	}

	constexpr void *EndOfBuffer() noexcept {
		return AtBuffer(buffer_size);
	}

	void *MakeFreeForFirstSlot(size_t required_space) noexcept {
		void *const first_slot = FirstSlot();

		while (true) {
			const auto &front_ = list->front();

			if (BytesDelta(first_slot, &front_) >= required_space)
				return first_slot;

			/* not enough room before the first item -
			   need to dispose it */
			assert(!empty());
			pop_front();
		}
	}

	void *MakeFree(size_t item_size) noexcept {
		assert(item_size <= buffer_size - sizeof(List));

		if (empty())
			return FirstSlot();

		auto first = list->begin();
		auto last = list->last();
		void *after_last = AfterLast(&*last);
		void *const end_of_buffer = EndOfBuffer();
		assert(after_last <= end_of_buffer);

		while (&*first > &*last) {
			/* wraparound - free space is somewhere in the
			   middle */

			if (BytesDelta(after_last, &*first) >= item_size)
				/* there's enough room between last
				   and first item */
				return after_last;

			/* not enough room; dispose the first item */
			pop_front();

			if (empty())
				return FirstSlot();

			first = list->begin();
		}

		/* no wraparound (anymore) */

		if (BytesDelta(after_last, end_of_buffer) >= item_size)
			/* there's enough room after the last item */
			return after_last;

		/* make room at the beginning of the buffer */
		return MakeFreeForFirstSlot(item_size);
	}
};
