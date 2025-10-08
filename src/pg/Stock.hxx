// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "stock/Class.hxx"
#include "stock/Stock.hxx"

namespace Pg {

class AsyncConnection;

/**
 * A #::Stock implementation which manages PostgreSQL connections
 * (#AsyncConnection instances).
 */
class Stock final : StockClass {
	class Item;

	::Stock stock;

	const std::string conninfo, schema;

public:
	Stock(EventLoop &event_loop,
	      const char *_conninfo, const char *_schema,
	      unsigned limit, unsigned max_idle) noexcept;

	operator ::Stock &() noexcept {
		return stock;
	}

	/**
	 * @see ::Stock::Shutdown()
	 */
	void Shutdown() noexcept {
		stock.Shutdown();
	}

	/**
	 * Cast a #StockItem obtained from this class to the
	 * underlying #AsyncConnection.  Of course, this reference is
	 * only valid until the #StockItem is returned to the
	 * #::Stock.
	 */
	static AsyncConnection &GetConnection(StockItem &item) noexcept;

private:
	/* virtual methods from class StockClass */
	void Create(CreateStockItem c, StockRequest request,
		    StockGetHandler &handler,
		    CancellablePointer &cancel_ptr) override;
};

} /* namespace Pg */
