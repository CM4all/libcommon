// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "Key.hxx"
#include "Options.hxx"
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

		const std::size_t hash;

		DeferEvent defer_empty{GetEventLoop(), BIND_THIS_METHOD(OnDeferredEmpty)};

		bool sticky = false;

		template<typename... Args>
		explicit Item(StockMap &_map, std::size_t _hash, Args&&... args) noexcept
			:Stock(std::forward<Args>(args)...), map(_map), hash(_hash) {}

	protected:
		virtual void OnEmpty() noexcept override {
			defer_empty.Schedule();
		}

	private:
		void OnDeferredEmpty() noexcept;

	public:
		struct GetKeyFunction {
			[[gnu::pure]]
			StockKey operator()(const Item &item) const noexcept {
				return StockKey{item.GetNameView(), item.hash};
			}
		};
	};

	static constexpr size_t N_BUCKETS = 4096;
	using Map =
		IntrusiveHashSet<Item, N_BUCKETS,
				 IntrusiveHashSetOperators<Item,
							   Item::GetKeyFunction,
							   std::hash<StockKey>,
							   std::equal_to<StockKey>>>;

	EventLoop &event_loop;

	StockClass &cls;

	/**
	 * Options for each stock.
	 */
	const StockOptions options;

	Map map;

public:
	StockMap(EventLoop &_event_loop, StockClass &_cls,
		 StockOptions _options) noexcept;

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
	void AddStats(StockStats &data) const noexcept;

	[[gnu::pure]]
	Stock &GetStock(StockKey key, const void *request) noexcept;

	/**
	 * Set the "sticky" flag.  Sticky stocks will not be deleted
	 * when they become empty.
	 */
	void SetSticky(Stock &stock, bool sticky) noexcept;

	void Get(StockKey key, StockRequest &&request,
		 StockGetHandler &handler,
		 CancellablePointer &cancel_ptr) noexcept {
		Stock &stock = GetStock(key, request.get());
		stock.Get(std::move(request), handler, cancel_ptr);
	}

	/**
	 * Obtains an item from the stock without going through the
	 * callback.  This requires a stock class which finishes the
	 * create() method immediately.
	 *
	 * Throws exception on error.
	 */
	StockItem *GetNow(StockKey key, StockRequest &&request) {
		Stock &stock = GetStock(key, request.get());
		return stock.GetNow(std::move(request));
	}

protected:
	/**
	 * Derived classes can override this method to choose
	 * different per-#Stock options.
	 */
	[[gnu::pure]]
	virtual StockOptions GetOptions([[maybe_unused]] const void *request,
					StockOptions o) const noexcept {
		return o;
	}
};
