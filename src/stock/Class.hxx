// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Request.hxx"

#include <cstdint>

class StockGetHandler;
class CancellablePointer;
struct CreateStockItem;

class StockClass {
public:
	/**
	 * May throw exception instead of calling InvokeCreateError().
	 */
	virtual void Create(CreateStockItem c,
			    StockRequest request,
			    StockGetHandler &handler,
			    CancellablePointer &cancel_ptr) = 0;

	/**
	 * Control whether creating a new item should be continued
	 * even if the caller cancels the operation.  Once that
	 * finishes, the new item is put on the "idle" list.  This
	 * feature can be useful if creating an item is expensive (and
	 * asynchronous), and canceling it throws away a considerable
	 * amount of effort which should better be used for the next
	 * request.
	 */
	[[gnu::pure]]
	virtual bool ShouldContinueOnCancel([[maybe_unused]] const void *request) const noexcept {
		return false;
	}

	/**
	 * @return if non-zero, then two consecutive requests with the
	 * same value are avoided (for fair scheduling)
	 */
	[[gnu::pure]]
	virtual uint_fast64_t GetFairnessHash([[maybe_unused]] const void *request) const noexcept {
		return 0;
	}
};
