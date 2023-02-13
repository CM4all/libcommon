// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "util/LeakDetector.hxx"
#include "util/IntrusiveList.hxx"

#include <exception>

class AbstractStock;
class StockGetHandler;

struct CreateStockItem {
	AbstractStock &stock;

	/**
	 * Wrapper for Stock::GetName()
	 */
	[[gnu::pure]]
	const char *GetStockName() const noexcept;

	/**
	 * Announce that the creation of this item has failed.
	 */
	void InvokeCreateError(StockGetHandler &handler,
			       std::exception_ptr ep) noexcept;

	/**
	 * Announce that the creation of this item has been aborted by the
	 * caller.
	 */
	void InvokeCreateAborted() noexcept;
};

class StockItem
	: LeakDetector
{
	IntrusiveListHook<IntrusiveHookMode::NORMAL> stock_item_siblings;

	AbstractStock &stock;

	/**
	 * If true, then this object will never be reused.
	 */
	bool fade = false;

public:
	using List =
		IntrusiveList<StockItem,
			      IntrusiveListMemberHookTraits<&StockItem::stock_item_siblings>,
			      true>;

	/**
	 * Kludge: this flag is true if this item is idle and is not yet
	 * in a "clean" state (e.g. a WAS process after STOP), and cannot
	 * yet be reused.  It will be postponed until this flag is false
	 * again.  TODO: replace this kludge.
	 */
	bool unclean = false;

#ifndef NDEBUG
	bool is_idle = false;
#endif

	explicit StockItem(CreateStockItem c) noexcept
		:stock(c.stock) {}

	StockItem(const StockItem &) = delete;
	StockItem &operator=(const StockItem &) = delete;

	virtual ~StockItem() noexcept;

	AbstractStock &GetStock() const noexcept {
		return stock;
	}

	/**
	 * Wrapper for Stock::GetName()
	 */
	[[gnu::pure]]
	const char *GetStockName() const noexcept;

	/**
	 * Return a busy item to the stock.  This is a wrapper for
	 * Stock::Put().
	 */
	void Put(bool destroy) noexcept;

	/**
	 * Prepare this item to be borrowed by a client.
	 *
	 * @return false when this item is defunct and shall be destroyed
	 */
	virtual bool Borrow() noexcept = 0;

	/**
	 * Return this borrowed item into the "idle" list.
	 *
	 * @return false when this item is defunct and shall not be reused
	 * again; it will be destroyed by the caller
	 */
	virtual bool Release() noexcept = 0;

	bool IsFading() const noexcept {
		return fade;
	}

	void Fade() noexcept {
		fade = true;
	}

	/**
	 * Announce that the creation of this item has finished
	 * successfully, and it is ready to be used.
	 */
	void InvokeCreateSuccess(StockGetHandler &handler) noexcept;

	/**
	 * Announce that the creation of this item has failed.
	 */
	void InvokeCreateError(StockGetHandler &handler,
			       std::exception_ptr ep) noexcept;

	/**
	 * Announce that the creation of this item has been aborted by the
	 * caller.
	 */
	void InvokeCreateAborted() noexcept;

	/**
	 * Announce that the item has been disconnected by the peer while
	 * it was idle.
	 */
	void InvokeIdleDisconnect() noexcept;

	/**
	 * Announce that the item has been disconnected by the peer while
	 * it was busy.
	 */
	void InvokeBusyDisconnect() noexcept;

	void ClearUncleanFlag() noexcept;
};
