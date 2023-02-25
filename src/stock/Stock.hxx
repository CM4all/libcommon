// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "BasicStock.hxx"
#include "Request.hxx"
#include "event/DeferEvent.hxx"
#include "util/IntrusiveList.hxx"

#include <cstddef>
#include <cstdint>

class CancellablePointer;
class Stock;
class StockClass;
class StockGetHandler;

/**
 * Objects in stock.  May be used for connection pooling.
 *
 * A #Stock instance holds a number of idle objects.
 */
class Stock : public BasicStock {
	/**
	 * The maximum number of items in this stock.  If any more items
	 * are requested, they are put into the #waiting list, which gets
	 * checked as soon as Put() is called.
	 */
	const std::size_t limit;

	/**
	 * This event is used to move the "retry waiting" code out of the
	 * current stack, to invoke the handler method in a safe
	 * environment.
	 */
	DeferEvent retry_event;

	struct Waiting;
	using WaitingList = IntrusiveList<Waiting>;

	WaitingList waiting;

	uint_least64_t last_fairness_hash = 0;

public:
	/**
	 * @param name may be something like a hostname:port pair for HTTP
	 * client connections - it is used for logging, and as a key by
	 * the #MapStock class
	 */
	gcc_nonnull(4)
	Stock(EventLoop &event_loop, StockClass &cls,
	      const char *name, std::size_t limit, std::size_t max_idle,
	      Event::Duration _clear_interval,
	      StockHandler *handler=nullptr) noexcept;

	~Stock() noexcept;

	Stock(const Stock &) = delete;
	Stock &operator=(const Stock &) = delete;

	/**
	 * @return true if the configured stock limit has been reached
	 * and no more items can be created, false if this stock is
	 * unlimited
	 */
	[[gnu::pure]]
	bool IsFull() const noexcept {
		return limit > 0 && GetActiveCount() >= limit;
	}

	void Get(StockRequest request,
		 StockGetHandler &get_handler,
		 CancellablePointer &cancel_ptr) noexcept;

	/**
	 * Obtains an item from the stock without going through the
	 * callback.  This requires a stock class which finishes the
	 * create() method immediately.
	 *
	 * Throws exception on error.
	 */
	StockItem *GetNow(StockRequest request);

	void Put(StockItem &item, bool destroy) noexcept override;

	/**
	 * Inject a newly created item into the "idle" list.
	 */
	void InjectIdle(StockItem &item) noexcept {
		BasicStock::InjectIdle(item);
		ScheduleRetryWaiting();
	}

	void ItemCreateError(StockGetHandler &get_handler,
			     std::exception_ptr ep) noexcept override;
	void ItemCreateAborted() noexcept override;

	void ItemUncleanFlagCleared() noexcept override {
		ScheduleRetryWaiting();
	}

private:
	[[gnu::pure]]
	WaitingList::iterator PickWaiting() noexcept;

	/**
	 * Retry the waiting requests.  This is called after the number of
	 * busy items was reduced.
	 */
	void RetryWaiting() noexcept;
	void ScheduleRetryWaiting() noexcept;
};
