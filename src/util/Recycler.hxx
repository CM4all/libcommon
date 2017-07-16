/*
 * author: Max Kellermann <mk@cm4all.com>
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
