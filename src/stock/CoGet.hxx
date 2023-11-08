// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Stock.hxx"
#include "GetHandler.hxx"
#include "co/AwaitableHelper.hxx"
#include "util/Cancellable.hxx"

/**
 * Coroutine wrapper for Stock::Get().
 */
class CoStockGet final : public StockGetHandler {
	CancellablePointer cancel_ptr{nullptr};

	StockItem *item = nullptr;

	std::exception_ptr error;

	std::coroutine_handle<> continuation;

	using Awaitable = Co::AwaitableHelper<CoStockGet>;
	friend Awaitable;

public:
	CoStockGet(Stock &stock, StockRequest request) noexcept {
		stock.Get(std::move(request), *this, cancel_ptr);
	}

	~CoStockGet() noexcept {
		if (cancel_ptr)
			cancel_ptr.Cancel();
		else if (item != nullptr)
			item->Put(PutAction::REUSE);
	}

	Awaitable operator co_await() noexcept {
		return Awaitable{*this};
	}

private:
	bool IsReady() const noexcept {
		return item != nullptr || error;
	}

	StockItem *TakeValue() noexcept {
		return std::exchange(item, nullptr);
	}

	/* virtual methods from StockGetHandler */
	void OnStockItemReady(StockItem &_item) noexcept override {
		cancel_ptr = nullptr;
		item = &_item;

		if (continuation)
			continuation.resume();
	}

	void OnStockItemError(std::exception_ptr ep) noexcept override {
		cancel_ptr = nullptr;
		error = std::move(ep);

		if (continuation)
			continuation.resume();
	}
};
