// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Stock.hxx"
#include "AsyncConnection.hxx"
#include "stock/Item.hxx"
#include "util/Cancellable.hxx"

#include <cassert>

namespace Pg {

class Stock::Item final : public StockItem, Cancellable, AsyncConnectionHandler {
	StockGetHandler &handler;

	AsyncConnection connection;

	bool initialized = false, idle;

	DeferEvent defer_initialized;

	std::exception_ptr error;

public:
	Item(CreateStockItem c, StockGetHandler &_handler,
	     EventLoop &event_loop,
	     const char *conninfo, const char *schema) noexcept
		:StockItem(c),
		 handler(_handler),
		 connection(event_loop, conninfo, schema, *this),
		 defer_initialized(event_loop,
				   BIND_THIS_METHOD(OnDeferredInitialized))
	{
		/* don't reconnect; this object will be destroyed on
		   disconnect and the next query will create a new
		   one */
		connection.DisableAutoReconnect();
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
			InvokeCreateError(handler, std::move(error));
		} else {
			idle = false;
			InvokeCreateSuccess(handler);
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
			InvokeBusyDisconnect();
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
			InvokeBusyDisconnect();
	}
};

void
Stock::Create(CreateStockItem c, StockRequest,
	      StockGetHandler &handler, CancellablePointer &cancel_ptr)
{
	auto *item = new Item(c, handler, stock.GetEventLoop(),
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
