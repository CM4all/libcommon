// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "Cast.hxx"
#include "IntrusiveForwardList.hxx"
#include "MemberIteratorAdapter.hxx"

#include <cassert>
#include <iterator>
#include <new>
#include <span>

/**
 * A fixed-size circular buffer which allocates variable-size items.
 *
 * The memory chunk used to store these items is managed by the
 * caller.  This class does not care for allocating or freeing it.
 */
template<typename T>
class VCircularBuffer {
public:
	using value_type = T;
	using reference = T &;
	using const_reference = const T &;
	using pointer = T *;
	using const_pointer = const T *;

private:
	struct Item {
		IntrusiveForwardListHook list_hook;

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

	using List = IntrusiveForwardList<
		Item,
		IntrusiveForwardListMemberHookTraits<&Item::list_hook>,
		IntrusiveForwardListOptions{.constant_time_size = true, .cache_last = true}>;

	List *const list;
	const size_t buffer_size;

	size_t tail_item_size;

public:
	VCircularBuffer(std::span<std::byte> buffer) noexcept
		:list(::new(Align(buffer, alignof(List)).data()) List()),
		 buffer_size(Align(buffer, alignof(List)).size()) {
		assert(buffer.size() >= sizeof(List) + sizeof(Item));
	}

	~VCircularBuffer() noexcept {
		clear();
		list->~List();
	}

	constexpr bool empty() const noexcept {
		return list->empty();
	}

	/**
	 * @return the number of items
	 */
	constexpr size_t size() const noexcept {
		return list->size();
	}

	/**
	 * @return the number of bytes used in the given buffer
	 */
	[[gnu::pure]]
	size_t GetMemoryUsage() const noexcept {
		if (empty())
			return sizeof(*list);

		const auto first = &*list->begin();
		const auto last = &*list->last();

		const auto after_last = (const std::byte *)AfterLast(&*last);

		if (first <= last)
			return after_last - (const std::byte *)first;

		const auto buffer_end = (const std::byte *)EndOfBuffer();
		return (after_last - (const std::byte *)list) + (buffer_end - (const std::byte *)first);
	}

	void clear() noexcept {
		list->clear_and_dispose(typename Item::Disposer());
	}

	constexpr reference front() noexcept {
		return list->front().value;
	}

	constexpr reference back() noexcept {
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
						     &Item::value>;

	constexpr const_iterator begin() const noexcept {
		return list->begin();
	}

	constexpr const_iterator end() const noexcept {
		return list->end();
	}

	constexpr const_iterator iterator_to(const_reference value) const noexcept {
		return list->iterator_to(Item::Cast(value));
	}

	using iterator = MemberIteratorAdapter<typename List::iterator,
					       &Item::value>;

	constexpr iterator begin() noexcept {
		return list->begin();
	}

	constexpr iterator end() noexcept {
		return list->end();
	}

	constexpr iterator iterator_to(reference value) noexcept {
		return list->iterator_to(Item::Cast(value));
	}

private:
	template<typename U>
	static constexpr U DeltaRoundUp(U p, U align) noexcept {
		return (align - (p % align)) % align;
	}

	static constexpr std::byte *Align(std::byte *p, size_t align) noexcept {
		return p + DeltaRoundUp((size_t)p, align);
	}

	static constexpr void *Align(void *p, size_t align) noexcept {
		return Align((std::byte *)p, align);
	}

	static constexpr void *Align(void *p) noexcept {
		return Align(p, alignof(Item));
	}

	static constexpr std::span<std::byte> Align(std::span<std::byte> src,
						    size_t align) noexcept {
		return {
			std::next(src.begin(),
				  DeltaRoundUp((size_t)src.data(), align)),
			src.end(),
		};
	}

	static constexpr std::span<std::byte> Align(std::span<std::byte> src) noexcept {
		return Align(src, alignof(Item));
	}

	static constexpr size_t ValueSizeToItemSize(size_t value_size) noexcept {
		return ContainerAttributeOffset<Item, T>(&Item::value) + value_size;
	}

	static constexpr size_t BytesDelta(const void *a, const void *b) noexcept {
		return static_cast<const std::byte *>(b)
			- static_cast<const std::byte *>(a);
	}

	constexpr void *FirstSlot() noexcept {
		return Align(list + 1);
	}

	constexpr void *AtBuffer(size_t offset) const noexcept {
		return OffsetPointer(list, offset);
	}

	constexpr void *AfterLast(Item *last) const noexcept {
		return Align(OffsetPointer(last, tail_item_size),
			     alignof(Item));
	}

	constexpr void *EndOfBuffer() const noexcept {
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
