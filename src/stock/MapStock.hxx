// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Stock.hxx"
#include "event/Chrono.hxx"
#include "event/DeferEvent.hxx"
#include "util/IntrusiveHashSet.hxx"

#include <concepts> // for std::predicate
#include <cstddef>

/**
 * A hash table of any number of Stock objects, each with a different
 * URI.
 */
class StockMap {
	struct Item final : IntrusiveHashSetHook<IntrusiveHookMode::NORMAL>, Stock {
		StockMap &map;

		DeferEvent defer_empty{GetEventLoop(), BIND_THIS_METHOD(OnDeferredEmpty)};

		bool sticky = false;

		template<typename... Args>
		explicit Item(StockMap &_map, Args&&... args) noexcept
			:Stock(std::forward<Args>(args)...), map(_map) {}

		template<typename... Args>
		explicit Item(Args&&... args) noexcept
			:Stock(std::forward<Args>(args)...) {}

	protected:
		virtual void OnEmpty() noexcept override {
			defer_empty.Schedule();
		}

	private:
		void OnDeferredEmpty() noexcept;

	public:
		struct Hash {
			[[gnu::pure]]
			size_t operator()(const char *key) const noexcept;
		};

		struct Equal {
			[[gnu::pure]]
			bool operator()(const char *a, const char *b) const noexcept;
		};

		struct GetKeyFunction {
			[[gnu::pure]]
			const char *operator()(const Item &item) const noexcept {
				return item.GetName();
			}
		};
	};

	static constexpr size_t N_BUCKETS = 251;
	using Map =
		IntrusiveHashSet<Item, N_BUCKETS,
				 IntrusiveHashSetOperators<Item,
							   Item::GetKeyFunction,
							   Item::Hash,
							   Item::Equal>>;

	EventLoop &event_loop;

	StockClass &cls;

	/**
	 * The maximum number of items in each stock.
	 */
	const std::size_t limit;

	/**
	 * The maximum number of permanent idle items in each stock.
	 */
	const std::size_t max_idle;

	const Event::Duration clear_interval;

	Map map;

public:
	StockMap(EventLoop &_event_loop, StockClass &_cls,
		 std::size_t _limit, std::size_t _max_idle,
		 Event::Duration _clear_interval) noexcept;

	~StockMap() noexcept;

	EventLoop &GetEventLoop() const noexcept {
		return event_loop;
	}

	StockClass &GetClass() noexcept {
		return cls;
	}

	void Erase(Item &item) noexcept;

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

	/**
	 * Obtain statistics.
	 */
	void AddStats(StockStats &data) const noexcept {
		map.for_each([&data](auto &i){
			i.AddStats(data);
		});
	}

	[[gnu::pure]]
	Stock &GetStock(const char *uri, const void *request) noexcept;

	/**
	 * Set the "sticky" flag.  Sticky stocks will not be deleted
	 * when they become empty.
	 */
	void SetSticky(Stock &stock, bool sticky) noexcept;

	void Get(const char *uri, StockRequest &&request,
		 StockGetHandler &handler,
		 CancellablePointer &cancel_ptr) noexcept {
		Stock &stock = GetStock(uri, request.get());
		stock.Get(std::move(request), handler, cancel_ptr);
	}

	/**
	 * Obtains an item from the stock without going through the
	 * callback.  This requires a stock class which finishes the
	 * create() method immediately.
	 *
	 * Throws exception on error.
	 */
	StockItem *GetNow(const char *uri, StockRequest &&request) {
		Stock &stock = GetStock(uri, request.get());
		return stock.GetNow(std::move(request));
	}

protected:
	/**
	 * Derived classes can override this method to choose a
	 * per-#Stock limit.
	 */
	[[gnu::pure]]
	virtual std::size_t GetLimit([[maybe_unused]] const void *request,
				     std::size_t _limit) const noexcept {
		return _limit;
	}

	/**
	 * Derived classes can override this method to choose a
	 * per-#Stock clear_interval.
	 */
	virtual Event::Duration GetClearInterval([[maybe_unused]] const void *request) const noexcept {
		return clear_interval;
	}
};
