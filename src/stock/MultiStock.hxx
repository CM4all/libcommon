// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Request.hxx"
#include "GetHandler.hxx"
#include "AbstractStock.hxx"
#include "Item.hxx"
#include "event/DeferEvent.hxx"
#include "event/CoarseTimerEvent.hxx"
#include "util/Cancellable.hxx"
#include "util/IntrusiveHashSet.hxx"
#include "util/IntrusiveList.hxx"

#include <concepts> // for std::predicate
#include <string>

class CancellablePointer;
class StockClass;
struct CreateStockItem;
class StockItem;
struct StockStats;

class MultiStockClass {
public:
	virtual std::size_t GetLimit(const void *request,
				     std::size_t _limit) const noexcept = 0;

	virtual Event::Duration GetClearInterval(const void *info) const noexcept = 0;

	virtual StockItem *Create(CreateStockItem c,
				  StockItem &shared_item) = 0;
};

/**
 * A #StockMap wrapper which allows multiple clients to use one
 * #StockItem.
 */
class MultiStock {
	class MapItem;

	/**
	 * A manager for an "outer" #StockItem which can be shared by
	 * multiple clients.
	 */
	class OuterItem final
		: IntrusiveListHook<IntrusiveHookMode::NORMAL>,
		  AbstractStock
	{
		friend struct IntrusiveListBaseHookTraits<OuterItem>;

		MapItem &parent;

		StockItem &shared_item;

		const std::size_t limit;

		/**
		 * This timer periodically deletes one third of all
		 * idle items, to get rid of all unused items
		 * eventually.
		 */
		CoarseTimerEvent cleanup_timer;

		const Event::Duration cleanup_interval;

		using ItemList = StockItem::List;

		ItemList idle, busy;

	public:
		OuterItem(MapItem &_parent, StockItem &_item,
			  std::size_t _limit,
			  Event::Duration _cleanup_interval) noexcept;

		OuterItem(const OuterItem &) = delete;
		OuterItem &operator=(const OuterItem &) = delete;

		~OuterItem() noexcept;

		bool IsItem(const StockItem &other) const noexcept {
			return &other == &shared_item;
		}

		bool IsFading() const noexcept {
			return shared_item.IsFading();
		}

		bool IsFull() const noexcept {
			return busy.size() >= limit;
		}

		bool CanCreateLease() const noexcept {
			return idle.size() + busy.size() < limit;
		}

		bool IsBusy() const noexcept {
			return !busy.empty();
		}

		bool IsEmpty() const noexcept {
			return idle.empty() && busy.empty();
		}

		bool CanUse() const noexcept;
		bool ShouldDelete() const noexcept;

		void DiscardUnused() noexcept;

		void Fade() noexcept;

		void FadeIf(std::predicate<const StockItem &> auto predicate) noexcept {
			if (predicate(shared_item))
				Fade();
		}

		void CreateLease(MultiStockClass &_cls,
				 StockGetHandler &handler) noexcept;

		/**
		 * @return true if a lease was submitted to the
		 * #StockGetHandler, false if the limit has been
		 * reached
		 */
		[[nodiscard]]
		bool GetLease(MultiStockClass &_cls,
			      StockGetHandler &handler) noexcept;

	private:
		StockItem *GetIdle() noexcept;

		bool GetIdle(StockGetHandler &handler) noexcept;

		void OnCleanupTimer() noexcept;

		void ScheduleCleanupTimer() noexcept {
			cleanup_timer.Schedule(cleanup_interval);
		}

		void ScheduleCleanupNow() noexcept {
			cleanup_timer.Schedule(Event::Duration::zero());
		}

		void CancelCleanupTimer() noexcept {
			cleanup_timer.Cancel();
		}

		/* virtual methods from class AbstractStock */
		const char *GetName() const noexcept override;
		EventLoop &GetEventLoop() const noexcept override;

		PutAction Put(StockItem &item, PutAction action) noexcept override;

		void ItemIdleDisconnect(StockItem &item) noexcept override;
		void ItemBusyDisconnect(StockItem &item) noexcept override;
		void ItemCreateSuccess(StockGetHandler &get_handler,
				       StockItem &item) noexcept override;
		void ItemCreateError(StockGetHandler &get_handler,
				     std::exception_ptr ep) noexcept override;
		void ItemUncleanFlagCleared() noexcept override;
	};

	class MapItem final
		: public IntrusiveHashSetHook<IntrusiveHookMode::AUTO_UNLINK>,
		  AbstractStock,
		  StockGetHandler
	{
		StockClass &outer_class;
		MultiStockClass &inner_class;

		const std::string name;

		using OuterItemList = IntrusiveList<
			OuterItem,
			IntrusiveListBaseHookTraits<OuterItem>,
			IntrusiveListOptions{.constant_time_size = true}>;

		OuterItemList items;

		/**
		 * The maximum number of items in this stock.  If any
		 * more items are requested, they are put into the
		 * #waiting list, which gets checked as soon as Put()
		 * is called.
		 */
		const std::size_t limit;

		const Event::Duration clear_interval;

		struct Waiting;
		using WaitingList = IntrusiveList<Waiting>;

		WaitingList waiting;

		/**
		 * This event is used to move the "retry_waiting" code
		 * out of the current stack, to invoke the handler
		 * method in a safe environment.
		 */
		DeferEvent retry_event;

		CancellablePointer get_cancel_ptr;

		std::size_t get_concurrency;

	public:
		/**
		 * For MultiStock::chronological_list.
		 */
		IntrusiveListHook<IntrusiveHookMode::AUTO_UNLINK> chronological_siblings;

		MapItem(EventLoop &event_loop, StockClass &_outer_class,
			std::string_view _name,
			std::size_t _limit,
			Event::Duration _clear_interval,
			MultiStockClass &_inner_class) noexcept;
		~MapItem() noexcept;

		bool IsEmpty() const noexcept {
			return items.empty() && waiting.empty();
		}

		std::size_t GetActiveCount() const noexcept {
			return items.size() + (bool)get_cancel_ptr;
		}

		/**
		 * @return true if the configured stock limit has been reached
		 * and no more items can be created, false if this stock is
		 * unlimited
		 */
		[[gnu::pure]]
		bool IsFull() const noexcept {
			return limit > 0 && GetActiveCount() >= limit;
		}

		void Get(StockRequest request, std::size_t concurrency,
			 StockGetHandler &handler,
			 CancellablePointer &cancel_ptr) noexcept;

		void RemoveWaiting(Waiting &w) noexcept;

		void RemoveItem(OuterItem &item) noexcept;
		void OnLeaseReleased(OuterItem &item) noexcept;

		/**
		 * @return the number of items that have been discarded
		 */
		std::size_t DiscardUnused() noexcept;

		void FadeAll() noexcept {
			for (auto &i : items)
				i.Fade();
		}

		void FadeIf(std::predicate<const StockItem &> auto predicate) noexcept {
			for (auto &i : items)
				i.FadeIf(predicate);
		}

	private:
		[[gnu::pure]]
		OuterItem &ToOuterItem(StockItem &shared_item) noexcept;

		[[gnu::pure]]
		OuterItem *FindUsable() noexcept;

		/**
		 * Delete all empty items.
		 */
		void DeleteEmptyItems(const OuterItem *except=nullptr) noexcept;

		void FinishWaiting(OuterItem &item) noexcept;

		/**
		 * Retry the waiting requests.
		 */
		void RetryWaiting() noexcept;
		bool ScheduleRetryWaiting() noexcept;

		void Create(StockRequest request) noexcept;

		/* virtual methods from class StockGetHandler */
		void OnStockItemReady(StockItem &item) noexcept override;
		void OnStockItemError(std::exception_ptr error) noexcept override;

		/* virtual methods from class AbstractStock */
		const char *GetName() const noexcept override {
			return name.c_str();
		}

		EventLoop &GetEventLoop() const noexcept override {
			return retry_event.GetEventLoop();
		}

		PutAction Put(StockItem &item, PutAction action) noexcept override;
		void ItemIdleDisconnect(StockItem &item) noexcept override;
		void ItemBusyDisconnect(StockItem &item) noexcept override;
		void ItemCreateSuccess(StockGetHandler &get_handler,
				       StockItem &item) noexcept override;
		void ItemCreateError(StockGetHandler &get_handler,
				     std::exception_ptr ep) noexcept override;

	public:
		struct Hash {
			[[gnu::pure]]
			std::size_t operator()(const char *key) const noexcept;
		};

		struct Equal {
			[[gnu::pure]]
			bool operator()(const char *a, const char *b) const noexcept;
		};

		struct GetKey {
			[[gnu::pure]]
			const char *operator()(const MapItem &item) const noexcept {
				return item.GetName();
			}
		};
	};

	EventLoop &event_loop;

	StockClass &outer_class;

	/**
	 * The maximum number of items in each stock.
	 */
	const std::size_t limit;

	MultiStockClass &inner_class;

	static constexpr size_t N_BUCKETS = 251;
	using Map =
		IntrusiveHashSet<MapItem, N_BUCKETS,
				 IntrusiveHashSetOperators<MapItem, MapItem::GetKey,
							   MapItem::Hash,
							   MapItem::Equal>>;

	Map map;

	/**
	 * A list that contains the most recently used items at the
	 * back and the least recently used items at the front.
	 *
	 * This is used by DiscardOldestIdle().
	 */
	IntrusiveList<MapItem,
		      IntrusiveListMemberHookTraits<&MapItem::chronological_siblings>> chronological_list;

public:
	MultiStock(EventLoop &_event_loop, StockClass &_outer_cls,
		   std::size_t _limit,
		   MultiStockClass &_inner_class) noexcept;
	~MultiStock() noexcept;

	MultiStock(const MultiStock &) = delete;
	MultiStock &operator=(const MultiStock &) = delete;

	auto &GetEventLoop() const noexcept {
		return event_loop;
	}

	/**
	 * Discard all items which are idle and havn't been used in a
	 * while.
	 *
	 * @return the number of items that have been discarded
	 */
	std::size_t DiscardUnused() noexcept;

	/**
	 * Discard a number of least recently used items which are
	 * idle.
	 *
	 * @param n the number of items to be removed
	 * @return the number of items actually discarded; this number
	 * can be slightly larger than #n because this method does not
	 * limit the number of clones removed
	 */
	std::size_t DiscardOldestIdle(std::size_t n) noexcept;

	/**
	 * @see Stock::FadeAll()
	 */
	void FadeAll() noexcept {
		map.for_each([](auto &i){
			i.FadeAll();
		});
	}

	/**
	 * @see Stock::FadeIf()
	 */
	void FadeIf(std::predicate<const StockItem &> auto predicate) noexcept {
		map.for_each([&predicate](auto &i){
			i.FadeIf(predicate);
		});
	}

	void Get(const char *uri, StockRequest request,
		 std::size_t concurrency,
		 StockGetHandler &handler,
		 CancellablePointer &cancel_ptr) noexcept;

private:
	[[gnu::pure]]
	MapItem &MakeMapItem(const char *uri, const void *request) noexcept;
};
