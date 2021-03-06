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
	      unsigned limit, unsigned max_idle) noexcept
		:stock(event_loop, *this, "Pg::AsyncConnection",
		       limit, max_idle, std::chrono::minutes(5)),
		 conninfo(_conninfo), schema(_schema)
	{
	}

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
		    CancellablePointer &cancel_ptr) override;
};

} /* namespace Pg */
