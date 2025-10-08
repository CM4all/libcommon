// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "MapStock.hxx"
#include "util/DeleteDisposer.hxx"

void
StockMap::Item::OnDeferredEmpty() noexcept
{
	if (IsEmpty() && !sticky)
		map.Erase(*this);
}

StockMap::StockMap(EventLoop &_event_loop, StockClass &_cls,
		   StockOptions _options) noexcept
	:event_loop(_event_loop), cls(_cls),
	 options(_options)
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
		const StockOptions item_options{
			.limit = GetLimit(request, options.limit),
			.max_idle = options.max_idle,
			.clear_interval = GetClearInterval(request),
		};

		auto *item = new Item(*this, key.hash, event_loop, cls,
				      key.value,
				      item_options);
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
