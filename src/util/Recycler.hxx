/*
 * Copyright 2007-2022 CM4all GmbH
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

#ifndef RECYCLER_HXX
#define RECYCLER_HXX

#include "Poison.hxx"

#include <boost/intrusive/slist.hpp>

#include <new>
#include <utility>

/**
 * Allocator for objects which recycles a certain number of
 * allocations.
 *
 * This class is not thread-safe.
 */
template<class T, size_t N>
class Recycler {
	typedef boost::intrusive::slist_member_hook<boost::intrusive::link_mode<boost::intrusive::normal_link>> Hook;

	union Item {
		Hook hook;

		T value;

		template<typename... Args>
		Item(Args&&... args)
			:value(std::forward<Args>(args)...) {}

		~Item() {}
	};

	boost::intrusive::slist<Item,
				boost::intrusive::member_hook<Item, Hook, &Item::hook>,
				boost::intrusive::constant_time_size<true>> list;

public:
	~Recycler() {
		Clear();
	}

	/**
	 * Free all objects in the recycler.
	 */
	void Clear() {
		list.clear_and_dispose([](Item *item){
				item->hook.~Hook();
				delete item;
			});
	}

	/**
	 * Allocate a new object, potentially taking the memory
	 * allocation from the recycler.  The instance must be freed
	 * with Put().
	 */
	template<typename... Args>
	T *Get(Args&&... args) {
		if (list.empty()) {
			auto *item = new Item(std::forward<Args>(args)...);
			return &item->value;
		} else {
			auto &item = list.front();
			list.pop_front();

			item.hook.~Hook();

			PoisonInaccessibleT(item);
			PoisonUndefinedT(item.value);

			try {
				new (&item.value) T(std::forward<Args>(args)...);
				return &item.value;
			} catch (...) {
				new (&item.hook) Hook();
				list.push_front(item);
				throw;
			}
		}
	}

	/**
	 * Free an instance allocated with Get().
	 */
	void Put(T *value) {
		auto *item = (Item *)value;
		item->value.~T();

		if (list.size() < N) {
			PoisonInaccessibleT(*item);
			PoisonUndefinedT(item->hook);

			new (&item->hook) Hook();
			list.push_front(*item);
		} else {
			delete item;
		}
	}
};

#endif
