// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "MultiStock.hxx"
#include "Class.hxx"
#include "GetHandler.hxx"
#include "Item.hxx"
#include "util/djb_hash.hxx"
#include "util/DeleteDisposer.hxx"
#include "util/StringAPI.hxx"

#include <cassert>

MultiStock::OuterItem::OuterItem(MapItem &_parent, StockItem &_item,
				 std::size_t _limit,
				 Event::Duration _cleanup_interval) noexcept
	:parent(_parent), shared_item(_item),
	 limit(_limit),
	 cleanup_timer(shared_item.GetStock().GetEventLoop(),
		       BIND_THIS_METHOD(OnCleanupTimer)),
	 cleanup_interval(_cleanup_interval)
{
}

MultiStock::OuterItem::~OuterItem() noexcept
{
	assert(busy.empty());

	DiscardUnused();

	shared_item.Put(true);
}

inline bool
MultiStock::OuterItem::CanUse() const noexcept
{
	return !shared_item.IsFading() && !IsFull();
}

inline bool
MultiStock::OuterItem::ShouldDelete() const noexcept
{
	return shared_item.IsFading() && IsEmpty();
}

void
MultiStock::OuterItem::OnCleanupTimer() noexcept
{
	if (IsEmpty()) {
		/* if this item was unused for one cleanup_timer
		   period, let parent.OnLeaseReleased() discard it */
		shared_item.Fade();
		parent.OnLeaseReleased(*this);
		return;
	}

	/* destroy one third of the idle items */
	for (std::size_t i = (idle.size() + 2) / 3; i > 0; --i)
		idle.pop_front_and_dispose(DeleteDisposer{});

	/* repeat until we need this OuterItem again or until there
	   are no more idle items */
	ScheduleCleanupTimer();
}

void
MultiStock::OuterItem::DiscardUnused() noexcept
{
	idle.clear_and_dispose(DeleteDisposer{});
}

void
MultiStock::OuterItem::Fade() noexcept
{
	shared_item.Fade();
	DiscardUnused();

	if (IsEmpty())
		/* let the parent destroy us */
		ScheduleCleanupNow();
}

inline void
MultiStock::OuterItem::CreateLease(MultiStockClass &_inner_class,
				   StockGetHandler &handler) noexcept
try {
	auto *lease = _inner_class.Create({*this}, shared_item);
	lease->InvokeCreateSuccess(handler);
} catch (...) {
	ItemCreateError(handler, std::current_exception());
}

inline StockItem *
MultiStock::OuterItem::GetIdle() noexcept
{
	assert(CanUse());

	// TODO code copied from Stock::GetIdle()

	auto i = idle.begin();
	const auto end = idle.end();
	while (i != end) {
		StockItem &item = *i;
		assert(item.is_idle);

		if (item.unclean) {
			/* postpone reusal of this item until it's
			   "clean" */
			// TODO: replace this kludge
			++i;
			continue;
		}

		i = idle.erase(i);

		if (item.Borrow()) {
#ifndef NDEBUG
			item.is_idle = false;
#endif

			CancelCleanupTimer();

			busy.push_front(item);
			return &item;
		}

		delete &item;
	}

	return nullptr;
}

inline bool
MultiStock::OuterItem::GetIdle(StockGetHandler &handler) noexcept
{
	assert(CanUse());

	auto *item = GetIdle();
	if (item == nullptr)
		return false;

	handler.OnStockItemReady(*item);
	return true;
}

void
MultiStock::OuterItem::GetLease(MultiStockClass &_inner_class,
				 StockGetHandler &handler) noexcept
{
	assert(CanUse());

	if (!GetIdle(handler))
		CreateLease(_inner_class, handler);
}

const char *
MultiStock::OuterItem::GetName() const noexcept
{
	return shared_item.GetStockName();
}

EventLoop &
MultiStock::OuterItem::GetEventLoop() const noexcept
{
	return cleanup_timer.GetEventLoop();
}

void
MultiStock::OuterItem::Put(StockItem &item, bool destroy) noexcept
{
	assert(!item.is_idle);
	assert(&item.GetStock() == this);

	assert(!busy.empty());

	busy.erase(busy.iterator_to(item));

	if (shared_item.IsFading() || destroy || item.IsFading() ||
	    !item.Release()) {
		delete &item;
	} else {
#ifndef NDEBUG
		item.is_idle = true;
#endif

		idle.push_front(item);

		ScheduleCleanupTimer();
	}

	parent.OnLeaseReleased(*this);
}

void
MultiStock::OuterItem::ItemIdleDisconnect(StockItem &item) noexcept
{
	assert(item.is_idle);
	assert(!idle.empty());

	idle.erase_and_dispose(idle.iterator_to(item), DeleteDisposer{});

	if (ShouldDelete())
		parent.RemoveItem(*this);
}

void
MultiStock::OuterItem::ItemBusyDisconnect(StockItem &item) noexcept
{
	assert(!item.is_idle);
	assert(!busy.empty());

	item.Fade();
}

void
MultiStock::OuterItem::ItemCreateSuccess(StockGetHandler &get_handler,
					 StockItem &item) noexcept
{
	busy.push_front(item);
	get_handler.OnStockItemReady(item);
}

void
MultiStock::OuterItem::ItemCreateError(StockGetHandler &get_handler,
					std::exception_ptr ep) noexcept
{
	Fade();

	if (IsEmpty())
		parent.OnLeaseReleased(*this);

	get_handler.OnStockItemError(std::move(ep));
}

void
MultiStock::OuterItem::ItemCreateAborted() noexcept
{
	// unreachable
}

void
MultiStock::OuterItem::ItemUncleanFlagCleared() noexcept
{
	parent.OnLeaseReleased(*this);
}

struct MultiStock::MapItem::Waiting final
	: IntrusiveListHook<IntrusiveHookMode::NORMAL>, Cancellable
{
	MapItem &parent;
	StockRequest request;
	StockGetHandler &handler;
	CancellablePointer &caller_cancel_ptr;

	Waiting(MapItem &_parent, StockRequest &&_request,
		StockGetHandler &_handler,
		CancellablePointer &_cancel_ptr) noexcept
		:parent(_parent), request(std::move(_request)),
		 handler(_handler),
		 caller_cancel_ptr(_cancel_ptr)
	{
		caller_cancel_ptr = *this;
	}

	/* virtual methods from class Cancellable */
	void Cancel() noexcept override {
		parent.RemoveWaiting(*this);
	}
};

MultiStock::MapItem::MapItem(EventLoop &_event_loop, StockClass &_outer_class,
			     const char *_name,
			     std::size_t _limit,
			     Event::Duration _clear_interval,
			     MultiStockClass &_inner_class) noexcept
	:outer_class(_outer_class),
	 inner_class(_inner_class),
	 name(_name),
	 limit(_limit),
	 clear_interval(_clear_interval),
	 retry_event(_event_loop, BIND_THIS_METHOD(RetryWaiting))
{
}

MultiStock::MapItem::~MapItem() noexcept
{
	assert(items.empty());
	assert(waiting.empty());

	if (get_cancel_ptr)
		get_cancel_ptr.Cancel();
}

MultiStock::OuterItem *
MultiStock::MapItem::FindUsable() noexcept
{
	for (auto i = items.begin(), end = items.end(); i != end;) {
		if (i->CanUse())
			return &*i;

		if (i->IsFading() && !i->IsBusy())
			/* as a kludge, this method disposes items
			   that have been marked "fading" somewhere
			   else without us noticing it */
			i = items.erase_and_dispose(i, DeleteDisposer{});
		else
			++i;
	}

	return nullptr;
}

inline void
MultiStock::MapItem::Create(StockRequest request) noexcept
{
	assert(!get_cancel_ptr);

	try {
		outer_class.Create({*this}, std::move(request),
				   *this, get_cancel_ptr);
	} catch (...) {
		ItemCreateError(*this, std::current_exception());
	}
}

inline void
MultiStock::MapItem::Get(StockRequest request, std::size_t concurrency,
			 StockGetHandler &get_handler,
			 CancellablePointer &cancel_ptr) noexcept
{
	if (auto *i = FindUsable()) {
		i->GetLease(inner_class, get_handler);
		return;
	}

	get_concurrency = concurrency;

	const bool waiting_empty = waiting.empty();

	auto *w = new Waiting(*this, std::move(request),
			      get_handler, cancel_ptr);
	waiting.push_back(*w);

	if (waiting_empty && !IsFull() && !get_cancel_ptr)
		Create(std::move(w->request));
}

inline void
MultiStock::MapItem::RemoveItem(OuterItem &item) noexcept
{
	items.erase_and_dispose(items.iterator_to(item), DeleteDisposer{});

	if (items.empty() && !ScheduleRetryWaiting())
		delete this;
}

inline void
MultiStock::MapItem::RemoveWaiting(Waiting &w) noexcept
{
	waiting.erase_and_dispose(waiting.iterator_to(w), DeleteDisposer{});

	if (!waiting.empty())
		return;

	if (retry_event.IsPending()) {
		/* an item is ready, but was not yet delivered to the
		   Waiting; delete all empty items */
		retry_event.Cancel();

		DeleteEmptyItems();
	}

	if (items.empty())
		delete this;
	else if (get_cancel_ptr)
		get_cancel_ptr.Cancel();
}

inline void
MultiStock::MapItem::DeleteEmptyItems(const OuterItem *except) noexcept
{
	items.remove_and_dispose_if([except](const auto &item){
		return &item != except && item.IsEmpty();
	}, DeleteDisposer{});
}

std::size_t
MultiStock::MapItem::DiscardUnused() noexcept
{
	return items.remove_and_dispose_if([](const auto &item){
		return !item.IsBusy();
	}, DeleteDisposer{});
}

inline void
MultiStock::MapItem::FinishWaiting(OuterItem &item) noexcept
{
	assert(item.CanUse());
	assert(!waiting.empty());
	assert(!retry_event.IsPending());

	auto &w = waiting.front();

	auto &get_handler = w.handler;
	waiting.pop_front_and_dispose(DeleteDisposer{});

	/* do it again until no more usable items are
	   found */
	if (!ScheduleRetryWaiting())
		/* no more waiting: we can now remove all
		   remaining idle items which havn't been
		   removed while there were still waiting
		   items, but we had more empty items than we
		   really needed */
		DeleteEmptyItems(&item);

	item.GetLease(inner_class, get_handler);
}

inline void
MultiStock::MapItem::RetryWaiting() noexcept
{
	assert(!waiting.empty());

	if (auto *i = FindUsable()) {
		FinishWaiting(*i);
		return;
	}

	auto &w = waiting.front();
	assert(w.request);

	if (!IsFull() && !get_cancel_ptr)
		Create(std::move(w.request));
}

bool
MultiStock::MapItem::ScheduleRetryWaiting() noexcept
{
	if (waiting.empty())
		return false;

	retry_event.Schedule();
	return true;
}

void
MultiStock::MapItem::OnStockItemReady(StockItem &stock_item) noexcept
{
	assert(!waiting.empty());
	get_cancel_ptr = nullptr;

	retry_event.Cancel();

	auto *item = new OuterItem(*this, stock_item, get_concurrency,
				   clear_interval);
	items.push_back(*item);

	FinishWaiting(*item);
}

void
MultiStock::MapItem::OnStockItemError(std::exception_ptr error) noexcept
{
	assert(!waiting.empty());
	get_cancel_ptr = nullptr;

	retry_event.Cancel();

	waiting.clear_and_dispose([&error](auto *w){
		w->handler.OnStockItemError(error);
		delete w;
	});

	if (items.empty())
		delete this;
}

inline MultiStock::OuterItem &
MultiStock::MapItem::ToOuterItem(StockItem &shared_item) noexcept
{
	auto i = std::find_if(items.begin(), items.end(), [&](const auto &j){
		return j.IsItem(shared_item);
	});

	assert(i != items.end());

	return *i;
}

void
MultiStock::MapItem::Put(StockItem &item, bool) noexcept
{
	assert(!item.is_idle);
	assert(&item.GetStock() == this);

	delete &item;
}

void
MultiStock::MapItem::ItemIdleDisconnect(StockItem &item) noexcept
{
	// should be unreachable because there are no idle items

	assert(item.is_idle);

	delete &item;
}

void
MultiStock::MapItem::ItemBusyDisconnect(StockItem &_item) noexcept
{
	assert(!_item.is_idle);

	/* this item will be destroyed by Put() */
	_item.Fade();

	auto &item = ToOuterItem(_item);

	if (!item.IsBusy())
		RemoveItem(item);
}


void
MultiStock::MapItem::ItemCreateSuccess(StockGetHandler &get_handler,
				       StockItem &item) noexcept
{
	get_handler.OnStockItemReady(item);
}

void
MultiStock::MapItem::ItemCreateError(StockGetHandler &get_handler,
				     std::exception_ptr ep) noexcept
{
	get_handler.OnStockItemError(ep);
}

void
MultiStock::MapItem::ItemCreateAborted() noexcept
{
	assert(!get_cancel_ptr);
}

inline void
MultiStock::MapItem::OnLeaseReleased(OuterItem &item) noexcept
{
	/* now that a lease was released, schedule the "waiting" list
	   again */
	if (ScheduleRetryWaiting() && item.CanUse())
		/* somebody's waiting and the iten can be reused for
		   them - don't try to delete the item, even if it's
		   empty */
		return;

	if (item.ShouldDelete())
		RemoveItem(item);
}

inline std::size_t
MultiStock::MapItem::Hash::operator()(const char *key) const noexcept
{
	assert(key != nullptr);

	return djb_hash_string(key);
}

inline bool
MultiStock::MapItem::Equal::operator()(const char *a, const char *b) const noexcept
{
	return StringIsEqual(a, b);
}

MultiStock::MultiStock(EventLoop &_event_loop, StockClass &_outer_cls,
		       std::size_t _limit,
		       MultiStockClass &_inner_class) noexcept
	:event_loop(_event_loop), outer_class(_outer_cls),
	 limit(_limit),
	 inner_class(_inner_class)
{
}

MultiStock::~MultiStock() noexcept
{
	DiscardUnused();

	/* by now, all leases must be freed */
	assert(map.empty());
}

std::size_t
MultiStock::DiscardUnused() noexcept
{
	std::size_t n = 0;

	map.remove_and_dispose_if([&n](auto &i){
		/* TODO this const_cast is an ugly kludge because the
		   predicate gets only a const reference */
		auto &j = const_cast<MapItem &>(i);
		n += j.DiscardUnused();

		return i.IsEmpty();
	}, DeleteDisposer{});

	return n;
}

std::size_t
MultiStock::DiscardOldestIdle(std::size_t n_requested) noexcept
{
	std::size_t n_removed = 0;

	for (auto &i : chronological_list) {
		n_removed += i.DiscardUnused();
		if (n_removed >= n_requested)
			break;
	}

	return n_removed;
}

MultiStock::MapItem &
MultiStock::MakeMapItem(const char *uri, void *request) noexcept
{
	auto [i, inserted] = map.insert_check(uri);
	if (inserted) {
		auto *item = new MapItem(GetEventLoop(), outer_class, uri,
					 inner_class.GetLimit(request, limit),
					 inner_class.GetClearInterval(request),
					 inner_class);
		map.insert_commit(i, *item);
		chronological_list.push_back(*item);
		return *item;
	} else {
		/* move to the back of chronological_list */
		i->chronological_siblings.unlink();
		chronological_list.push_back(*i);

		return *i;
	}

}

void
MultiStock::Get(const char *uri, StockRequest request,
		std::size_t concurrency,
		StockGetHandler &handler,
		CancellablePointer &cancel_ptr) noexcept
{
	MakeMapItem(uri, request.get())
		.Get(std::move(request), concurrency,
		     handler, cancel_ptr);
}
