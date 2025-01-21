// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "MapStock.hxx"
#include "util/DeleteDisposer.hxx"

void
StockMap::Item::OnDeferredEmpty() noexcept
{
	if (IsEmpty() && !sticky)
		map.Erase(*this);
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

Stock &
StockMap::GetStock(StockKey key, const void *request) noexcept
{
	auto [position, inserted] = map.insert_check(key);
	if (inserted) {
		auto *item = new Item(*this, key.hash, event_loop, cls,
				      key.value,
				      GetLimit(request, limit),
				      max_idle,
				      GetClearInterval(request));
		map.insert_commit(position, *item);
		return *item;
	} else
		return *position;
}

void
StockMap::SetSticky(Stock &stock, bool sticky) noexcept
{
	auto &item = static_cast<Item &>(stock);
	if (!sticky && stock.IsEmpty()) {
		Erase(item);
		return;
	}

	item.sticky = sticky;
}
