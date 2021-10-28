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

#include "Stock.hxx"
#include "AsyncConnection.hxx"
#include "stock/Item.hxx"
#include "util/Cancellable.hxx"

#include <cassert>

namespace Pg {

class Stock::Item final : public StockItem, Cancellable, AsyncConnectionHandler {
	AsyncConnection connection;

	bool initialized = false, idle;

	DeferEvent defer_initialized;

	std::exception_ptr error;

public:
	Item(CreateStockItem c, EventLoop &event_loop,
	     const char *conninfo, const char *schema) noexcept
		:StockItem(c),
		 connection(event_loop, conninfo, schema, *this),
		 defer_initialized(event_loop,
				   BIND_THIS_METHOD(OnDeferredInitialized))
	{
	}

	void Connect(CancellablePointer &cancel_ptr) noexcept {
		assert(!initialized);

		cancel_ptr = *this;
		connection.Connect();
	}

	auto &GetConnection() noexcept {
		return connection;
	}

private:
	void OnDeferredInitialized() noexcept {
		assert(initialized);

		if (error) {
			InvokeCreateError(std::move(error));
		} else {
			idle = false;
			InvokeCreateSuccess();
		}
	}

	/* virtual methods from class StockItem */
	bool Borrow() noexcept override {
		assert(initialized);
		assert(idle);

		idle = false;
		return true;
	}

	bool Release() noexcept override {
		assert(initialized);
		assert(!idle);

		idle = true;
		unclean = connection.IsCancelling();
		return true;
	}

	/* virtual methods from class Cancellable */
	void Cancel() noexcept override {
		assert(!initialized || defer_initialized.IsPending());

		InvokeCreateAborted();
	}

	/* virtual methods from class AsyncConnectionHandler */
	void OnConnect() override {
		if (!initialized) {
			initialized = true;
			defer_initialized.Schedule();
		}
	}

	void OnIdle() override {
		if (unclean)
			ClearUncleanFlag();
	}

	void OnDisconnect() noexcept override {
		if (!initialized || defer_initialized.IsPending()) {
			if (!error)
				error = std::make_exception_ptr(std::runtime_error("Disconnected"));
			initialized = true;
			defer_initialized.Schedule();
		} else if (idle)
			InvokeIdleDisconnect();
		else
			fade = true;
	}

	void OnNotify(const char *) override {}

	void OnError(std::exception_ptr e) noexcept override {
		if (!initialized) {
			error = std::move(e);
			initialized = true;
			defer_initialized.Schedule();
		} else if (idle)
			InvokeIdleDisconnect();
		else
			fade = true;
	}
};

void
Stock::Create(CreateStockItem c, StockRequest, CancellablePointer &cancel_ptr)
{
	auto *item = new Item(c, stock.GetEventLoop(),
			      conninfo.c_str(), schema.c_str());
	item->Connect(cancel_ptr);
}

AsyncConnection &
Stock::GetConnection(StockItem &_item) noexcept
{
	auto &item = (Item &)_item;
	return item.GetConnection();
}

} /* namespace Pg */
