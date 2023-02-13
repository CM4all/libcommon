// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "MapStock.hxx"
#include "util/djbhash.h"
#include "util/DeleteDisposer.hxx"

inline size_t
StockMap::Item::KeyHasher(const char *key) noexcept
{
	assert(key != nullptr);

	return djb_hash_string(key);
}

StockMap::StockMap(EventLoop &_event_loop, StockClass &_cls,
		   std::size_t _limit, std::size_t _max_idle,
		   Event::Duration _clear_interval) noexcept
	:event_loop(_event_loop), cls(_cls),
	 limit(_limit), max_idle(_max_idle),
	 clear_interval(_clear_interval)
{
}

StockMap::~StockMap() noexcept
{
	map.clear_and_dispose(DeleteDisposer());
}

void
StockMap::Erase(Item &item) noexcept
{
	auto i = map.iterator_to(item);
	map.erase_and_dispose(i, DeleteDisposer());
}

void
StockMap::OnStockEmpty(Stock &stock) noexcept
{
	auto &item = Item::Cast(stock);
	if (item.sticky)
		return;

	logger.Format(5, "hstock(%p) remove empty stock(%p, '%s')",
		      (const void *)this, (const void *)&stock, stock.GetName());

	Erase(item);
}

Stock &
StockMap::GetStock(const char *uri, void *request) noexcept
{
	auto [position, inserted] = map.insert_check(uri);
	if (inserted) {
		auto *item = new Item(event_loop, cls,
				      uri,
				      GetLimit(request, limit),
				      max_idle,
				      GetClearInterval(request),
				      this);
		map.insert(position, *item);
		return item->stock;
	} else
		return position->stock;
}

void
StockMap::SetSticky(Stock &stock, bool sticky) noexcept
{
	auto &item = Item::Cast(stock);
	if (!sticky && stock.IsEmpty()) {
		Erase(item);
		return;
	}

	item.sticky = sticky;
}
