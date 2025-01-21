// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Item.hxx"
#include "Stock.hxx"

std::string_view
CreateStockItem::GetStockNameView() const noexcept
{
	return stock.GetNameView();
}

const char *
CreateStockItem::GetStockNameC() const noexcept
{
	return stock.GetNameC();
}

void
CreateStockItem::InvokeCreateError(StockGetHandler &handler,
				   std::exception_ptr ep) noexcept
{
	stock.ItemCreateError(handler, ep);
}

StockItem::~StockItem() noexcept
{
}

std::string_view
StockItem::GetStockNameView() const noexcept
{
	return stock.GetNameView();
}

const char *
StockItem::GetStockNameC() const noexcept
{
	return stock.GetNameC();
}

PutAction
StockItem::Put(PutAction action) noexcept
{
	return stock.Put(*this, action);
}

void
StockItem::InvokeCreateSuccess(StockGetHandler &handler) noexcept
{
	stock.ItemCreateSuccess(handler, *this);
}

void
StockItem::InvokeCreateError(StockGetHandler &handler,
			     std::exception_ptr ep) noexcept
{
	stock.ItemCreateError(*this, handler, ep);
}

void
StockItem::InvokeIdleDisconnect() noexcept
{
	stock.ItemIdleDisconnect(*this);
}

void
StockItem::InvokeBusyDisconnect() noexcept
{
	stock.ItemBusyDisconnect(*this);
}

void
StockItem::ClearUncleanFlag() noexcept
{
	assert(unclean);
	unclean = false;

	stock.ItemUncleanFlagCleared();
}
