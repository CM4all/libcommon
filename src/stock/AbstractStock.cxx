// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "AbstractStock.hxx"
#include "Item.hxx"

void
AbstractStock::ItemCreateError(StockItem &item, StockGetHandler &get_handler,
			       std::exception_ptr ep) noexcept
{
	ItemCreateError(get_handler, ep);
	delete &item;
}

void
AbstractStock::ItemCreateAborted(StockItem &item) noexcept
{
	ItemCreateAborted();
	delete &item;
}
