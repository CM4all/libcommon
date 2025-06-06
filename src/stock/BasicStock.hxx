// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "AbstractStock.hxx"
#include "Item.hxx"
#include "Request.hxx"
#include "Stats.hxx"
#include "event/CoarseTimerEvent.hxx"
#include "util/DeleteDisposer.hxx"
#include "util/IntrusiveList.hxx"

#include <concepts> // for std::predicate
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

class CancellablePointer;
class BasicStock;
class StockClass;
class StockGetHandler;

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

	CoarseTimerEvent cleanup_event;
	CoarseTimerEvent clear_event;

	using ItemList = StockItem::List;

	/**
	 * All items that are currently idle.  Once an item gets
	 * borrowed, it gets moved to #busy.
	 */
	ItemList idle;

	/**
	 * All items that are currently busy (i.e. borrowed).  It will
	 * eventually be returned by calling Put(), which either moves
	 * it back to #idle or destroys it.
	 */
	ItemList busy;

	struct Create;

	/**
	 * The items that are currently being created.  We keep track
	 * of this because we need to know whether this stock is empty
	 * (see OnEmpty()) and whether this stock is full (see
	 * Stock::IsFull()).
	 */
	IntrusiveList<Create,
		IntrusiveListBaseHookTraits<Create>,
		IntrusiveListOptions{.constant_time_size = true}> create;

protected:
	bool may_clear = false;

public:
	/**
	 * @param name may be something like a hostname:port pair for HTTP
	 * client connections - it is used for logging, and as a key by
	 * the #MapStock class
	 */
	BasicStock(EventLoop &event_loop, StockClass &cls,
		   std::string_view name, std::size_t max_idle,
		   Event::Duration _clear_interval) noexcept;

	~BasicStock() noexcept;

	BasicStock(const BasicStock &) = delete;
	BasicStock &operator=(const BasicStock &) = delete;

	EventLoop &GetEventLoop() const noexcept override {
		return cleanup_event.GetEventLoop();
	}

	StockClass &GetClass() noexcept {
		return cls;
	}

	std::string_view GetNameView() const noexcept override {
		return name;
	}

	const char *GetNameC() const noexcept override {
		return name.c_str();
	}

	/**
	 * Returns true if there are no items in the stock - neither idle
	 * nor busy.
	 */
	[[gnu::pure]]
	bool IsEmpty() const noexcept {
		return idle.empty() && busy.empty() && create.empty();
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
	void FadeIf(std::predicate<const StockItem &> auto predicate) noexcept {
		for (auto &i : busy)
			if (predicate(i))
				i.Fade();

		ClearIdleIf(predicate);

		CheckEmpty();
		// TODO: restart the "create" list?
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
	 * and the ones being created (#create).  This number is used
	 * to compare with the configured #limit.
	 */
	std::size_t GetActiveCount() const noexcept {
		return busy.size() + create.size();
	}

	virtual void OnCreateCanceled() noexcept {}

	/**
	 * The stock has become empty.  It is not safe to delete it
	 * from within this method.
	 */
	virtual void OnEmpty() noexcept {}

private:
	/**
	 * Check if the stock has become empty, and invoke the handler.
	 */
	void CheckEmpty() noexcept;

	void ScheduleClear() noexcept {
		if (clear_interval > Event::Duration::zero())
			clear_event.Schedule(clear_interval);
	}

	void ClearIdle() noexcept;

	void ClearIdleIf(std::predicate<const StockItem &> auto predicate) noexcept {
		idle.remove_and_dispose_if(predicate,
					   DeleteDisposer());

		if (idle.size() <= max_idle)
			UnscheduleCleanup();
	}

protected:
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
	 *
	 * @param discard_request a #StockRequest that shall be
         * discarded before invoking the #StockGetHandler
	 */
	bool GetIdle(StockRequest &discard_request,
		     StockGetHandler &handler) noexcept;

	/**
	 * Attach the handler to a canceled create request.
	 *
	 * @return true on success, false if no canceled create
	 * requests exists
	 */
	bool GetCanceled(StockGetHandler &get_handler,
			 CancellablePointer &cancel_ptr) noexcept;

	void GetCreate(StockRequest request,
		       StockGetHandler &get_handler,
		       CancellablePointer &cancel_ptr) noexcept;

	PutAction Put(StockItem &item, PutAction action) noexcept override;

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

private:
	void DeleteCreate(Create &c) noexcept;
	void CreateCanceled(Create &c) noexcept;

	void ScheduleCleanup() noexcept {
		cleanup_event.Schedule(std::chrono::seconds(20));
	}

	void UnscheduleCleanup() noexcept {
		cleanup_event.Cancel();
	}

	void CleanupEventCallback() noexcept;
	void ClearEventCallback() noexcept;
};
