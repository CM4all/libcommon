// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Stock.hxx"
#include "GetHandler.hxx"
#include "co/Compat.hxx"
#include "util/Cancellable.hxx"

/**
 * Coroutine wrapper for Stock::Get().
 */
class CoStockGet final : public StockGetHandler {
	CancellablePointer cancel_ptr{nullptr};

	StockItem *item = nullptr;

	std::exception_ptr error;

	std::coroutine_handle<> continuation;

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

	auto operator co_await() noexcept {
		struct Awaitable final {
			CoStockGet &get;

			bool await_ready() const noexcept {
				return get.item != nullptr || get.error;
			}

			std::coroutine_handle<> await_suspend(std::coroutine_handle<> _continuation) const noexcept {
				get.continuation = _continuation;
				return std::noop_coroutine();
			}

			StockItem *await_resume() {
				if (get.error)
					std::rethrow_exception(get.error);

				return std::exchange(get.item, nullptr);
			}
		};

		return Awaitable{*this};
	}

private:
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
