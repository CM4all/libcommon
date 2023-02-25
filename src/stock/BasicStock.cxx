// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Stock.hxx"
#include "Class.hxx"
#include "GetHandler.hxx"
#include "util/Cancellable.hxx"

#include <cassert>

void
BasicStock::DiscardUnused() noexcept
{
	if (clear_interval > Event::Duration::zero() && !may_clear)
		return;

	ClearIdle();

	may_clear = true;
	ScheduleClear();
	ScheduleCheckEmpty();
}

void
BasicStock::FadeAll() noexcept
{
	for (auto &i : busy)
		i.Fade();

	ClearIdle();
	ScheduleCheckEmpty();

	// TODO: restart the "num_create" list?
}

void
BasicStock::Shutdown() noexcept
{
	FadeAll();

	cleanup_event.Cancel();
	clear_event.Cancel();
}

/*
 * The "empty()" handler method.
 *
 */

void
BasicStock::CheckEmpty() noexcept
{
	if (IsEmpty() && handler != nullptr)
		handler->OnStockEmpty(*this);
}

void
BasicStock::ScheduleCheckEmpty() noexcept
{
	if (IsEmpty() && handler != nullptr)
		empty_event.Schedule();
}


/*
 * cleanup
 *
 */

void
BasicStock::CleanupEventCallback() noexcept
{
	assert(idle.size() > max_idle);

	/* destroy one third of the idle items */

	for (std::size_t i = (idle.size() - max_idle + 2) / 3; i > 0; --i)
		idle.pop_front_and_dispose(DeleteDisposer());

	/* schedule next cleanup */

	if (idle.size() > max_idle)
		ScheduleCleanup();
	else
		CheckEmpty();
}

/*
 * clear after 60 seconds idle
 *
 */

void
BasicStock::ClearIdle() noexcept
{
	logger.Format(5, "ClearIdle num_idle=%zu num_busy=%zu",
		      idle.size(), busy.size());

	if (idle.size() > max_idle)
		UnscheduleCleanup();

	idle.clear_and_dispose(DeleteDisposer());
}

void
BasicStock::ClearEventCallback() noexcept
{
	logger.Format(6, "ClearEvent may_clear=%d", may_clear);

	if (may_clear)
		ClearIdle();

	may_clear = true;
	ScheduleClear();
	CheckEmpty();
}


/*
 * constructor
 *
 */

BasicStock::BasicStock(EventLoop &event_loop, StockClass &_cls,
		       const char *_name, std::size_t _max_idle,
		       Event::Duration _clear_interval,
		       StockHandler *_handler) noexcept
	:cls(_cls),
	 name(_name),
	 max_idle(_max_idle),
	 clear_interval(_clear_interval),
	 handler(_handler),
	 logger(name),
	 empty_event(event_loop, BIND_THIS_METHOD(CheckEmpty)),
	 cleanup_event(event_loop, BIND_THIS_METHOD(CleanupEventCallback)),
	 clear_event(event_loop, BIND_THIS_METHOD(ClearEventCallback))
{
	assert(max_idle > 0);

	ScheduleClear();
}

BasicStock::~BasicStock() noexcept
{
	assert(num_create == 0);

	/* must not delete the Stock when there are busy items left */
	assert(busy.empty());

	empty_event.Cancel();
	cleanup_event.Cancel();
	clear_event.Cancel();

	ClearIdle();
}

StockItem *
BasicStock::GetIdle() noexcept
{
	auto i = idle.begin();
	const auto end = idle.end();
	while (i != end) {
		StockItem &item = *i;
		assert(item.is_idle);

		if (item.unclean) {
			/* postpone reusal of this item until it's "clean" */
			// TODO: replace this kludge
			++i;
			continue;
		}

		i = idle.erase(i);

		if (idle.size() == max_idle)
			UnscheduleCleanup();

		if (item.Borrow()) {
#ifndef NDEBUG
			item.is_idle = false;
#endif

			busy.push_front(item);
			return &item;
		}

		delete &item;
	}

	ScheduleCheckEmpty();
	return nullptr;
}

bool
BasicStock::GetIdle(StockRequest &request,
		    StockGetHandler &get_handler) noexcept
{
	auto *item = GetIdle();
	if (item == nullptr)
		return false;


	/* destroy the request before invoking the handler, because
	   the handler may destroy the memory pool, which may
	   invalidate the request's memory region */
	request.reset();

	get_handler.OnStockItemReady(*item);
	return true;
}

void
BasicStock::GetCreate(StockRequest request,
		      StockGetHandler &get_handler,
		      CancellablePointer &cancel_ptr) noexcept
{
	++num_create;

	try {
		cls.Create({*this}, std::move(request),
			   get_handler, cancel_ptr);
	} catch (...) {
		ItemCreateError(get_handler, std::current_exception());
	}
}

void
BasicStock::ItemCreateSuccess(StockGetHandler &get_handler,
			      StockItem &item) noexcept
{
	assert(num_create > 0);
	--num_create;

	busy.push_front(item);

	get_handler.OnStockItemReady(item);
}

void
BasicStock::ItemCreateError(StockGetHandler &get_handler,
			    std::exception_ptr ep) noexcept
{
	assert(num_create > 0);
	--num_create;

	ScheduleCheckEmpty();

	get_handler.OnStockItemError(ep);
}

void
BasicStock::ItemCreateAborted() noexcept
{
	assert(num_create > 0);
	--num_create;

	ScheduleCheckEmpty();
}

void
BasicStock::Put(StockItem &item, bool destroy) noexcept
{
	assert(!item.is_idle);
	assert(&item.GetStock() == this);

	may_clear = false;

	assert(!busy.empty());

	busy.erase(busy.iterator_to(item));

	if (destroy || item.IsFading() || !item.Release()) {
		delete &item;
		ScheduleCheckEmpty();
	} else {
		InjectIdle(item);
	}
}

void
BasicStock::InjectIdle(StockItem &item) noexcept
{
	assert(!item.is_idle);
	assert(&item.GetStock() == this);

#ifndef NDEBUG
	item.is_idle = true;
#endif

	if (idle.size() == max_idle)
		ScheduleCleanup();

	idle.push_front(item);
}

void
BasicStock::ItemIdleDisconnect(StockItem &item) noexcept
{
	assert(item.is_idle);

	assert(!idle.empty());

	idle.erase(idle.iterator_to(item));

	if (idle.size() == max_idle)
		UnscheduleCleanup();

	delete &item;
	ScheduleCheckEmpty();
}

void
BasicStock::ItemBusyDisconnect(StockItem &item) noexcept
{
	assert(!item.is_idle);

	/* this item will be destroyed by Put() */
	item.Fade();
}
