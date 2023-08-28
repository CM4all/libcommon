// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "AbstractStock.hxx"
#include "Item.hxx"
#include "Request.hxx"
#include "Stats.hxx"
#include "event/CoarseTimerEvent.hxx"
#include "event/DeferEvent.hxx"
#include "util/DeleteDisposer.hxx"
#include "util/IntrusiveList.hxx"

#include <cstddef>
#include <cstdint>
#include <string>

class CancellablePointer;
class BasicStock;
class StockClass;
class StockGetHandler;

class StockHandler {
public:
	/**
	 * The stock has become empty.  It is safe to delete it from
	 * within this method.
	 */
	virtual void OnStockEmpty(BasicStock &stock) noexcept = 0;
};

/**
 * Objects in stock.  May be used for connection pooling.
 *
 * A #Stock instance holds a number of idle objects.
 */
class BasicStock : public AbstractStock {
protected:
	StockClass &cls;

private:
	const std::string name;

	/**
	 * The maximum number of permanent idle items.  If there are more
	 * than that, a timer will incrementally kill excess items.
	 */
	const std::size_t max_idle;

	const Event::Duration clear_interval;

	StockHandler *const handler;

	/**
	 * This event is used to move the "empty" check out of the current
	 * stack, to invoke the handler method in a safe environment.
	 */
	DeferEvent empty_event;

	CoarseTimerEvent cleanup_event;
	CoarseTimerEvent clear_event;

	using ItemList = StockItem::List;

	ItemList idle;

	ItemList busy;

	std::size_t num_create = 0;

protected:
	bool may_clear = false;

public:
	/**
	 * @param name may be something like a hostname:port pair for HTTP
	 * client connections - it is used for logging, and as a key by
	 * the #MapStock class
	 */
	BasicStock(EventLoop &event_loop, StockClass &cls,
		   const char *name, std::size_t max_idle,
		   Event::Duration _clear_interval,
		   StockHandler *handler=nullptr) noexcept;

	~BasicStock() noexcept;

	BasicStock(const BasicStock &) = delete;
	BasicStock &operator=(const BasicStock &) = delete;

	EventLoop &GetEventLoop() const noexcept override {
		return empty_event.GetEventLoop();
	}

	StockClass &GetClass() noexcept {
		return cls;
	}

	const char *GetName() const noexcept override {
		return name.c_str();
	}

	/**
	 * Returns true if there are no items in the stock - neither idle
	 * nor busy.
	 */
	[[gnu::pure]]
	bool IsEmpty() const noexcept {
		return idle.empty() && busy.empty() && num_create == 0;
	}

	/**
	 * Obtain statistics.
	 */
	void AddStats(StockStats &data) const noexcept {
		data.busy += busy.size();
		data.idle += idle.size();
	}

	/**
	 * Destroy all idle items and don't reuse any of the current busy
	 * items.
	 */
	void FadeAll() noexcept;

	/**
	 * Destroy all matching idle items and don't reuse any of the
	 * matching busy items.
	 */
	template<typename P>
	void FadeIf(P &&predicate) noexcept {
		for (auto &i : busy)
			if (predicate(i))
				i.Fade();

		ClearIdleIf(std::forward<P>(predicate));

		ScheduleCheckEmpty();
		// TODO: restart the "num_create" list?
	}

	/**
	 * Enable shutdown mode where all returned items will be
	 * destroyed and all events will be deregistered.
	 */
	void Shutdown() noexcept;

protected:
	bool HasIdle() const noexcept {
		return !idle.empty();
	}

	/**
	 * Determine the number of "active" items, i.e. the busy items
	 * and the ones being created (#num_create).  This number is
	 * used to compare with the configured #limit.
	 */
	std::size_t GetActiveCount() const noexcept {
		return busy.size() + num_create;
	}

private:
	/**
	 * Check if the stock has become empty, and invoke the handler.
	 */
	void CheckEmpty() noexcept;
	void ScheduleCheckEmpty() noexcept;

	void ScheduleClear() noexcept {
		if (clear_interval > Event::Duration::zero())
			clear_event.Schedule(clear_interval);
	}

	void ClearIdle() noexcept;

	template<typename P>
	void ClearIdleIf(P &&predicate) noexcept {
		idle.remove_and_dispose_if(std::forward<P>(predicate),
					   DeleteDisposer());

		if (idle.size() <= max_idle)
			UnscheduleCleanup();
	}

public:
	/**
	 * Borrow an idle item.
	 *
	 * @return an item or nullptr if there was no (usable) idle
	 * item
	 */
	StockItem *GetIdle() noexcept;

	/**
	 * @param request a request that shall be destroyed before
	 * invoking the handler (to avoid use-after-free bugs)
	 */
	bool GetIdle(StockRequest &request,
		     StockGetHandler &handler) noexcept;

	void GetCreate(StockRequest request,
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

	void Put(StockItem &item, PutAction action) noexcept override;

	/**
	 * Inject a newly created item into the "idle" list.
	 */
	void InjectIdle(StockItem &item) noexcept;

	void ItemIdleDisconnect(StockItem &item) noexcept override;
	void ItemBusyDisconnect(StockItem &item) noexcept override;
	void ItemCreateSuccess(StockGetHandler &get_handler,
			       StockItem &item) noexcept override;
	void ItemCreateError(StockGetHandler &get_handler,
			     std::exception_ptr ep) noexcept override;
	void ItemCreateAborted() noexcept override;

private:
	void ScheduleCleanup() noexcept {
		cleanup_event.Schedule(std::chrono::seconds(20));
	}

	void UnscheduleCleanup() noexcept {
		cleanup_event.Cancel();
	}

	void CleanupEventCallback() noexcept;
	void ClearEventCallback() noexcept;
};
