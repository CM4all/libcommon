/*
 * Copyright 2007-2021 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "Stock.hxx"
#include "GetHandler.hxx"
#include "co/Compat.hxx"

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
			item->Put(false);
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
