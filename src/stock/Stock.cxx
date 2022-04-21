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

#include "Stock.hxx"
#include "Class.hxx"
#include "GetHandler.hxx"
#include "util/Cancellable.hxx"

#include <cassert>

struct Stock::Waiting final
	: boost::intrusive::list_base_hook<boost::intrusive::link_mode<boost::intrusive::normal_link>>,
	     Cancellable
{
	Stock &stock;

	StockRequest request;

	const uint_least64_t fairness_hash;

	StockGetHandler &handler;

	CancellablePointer &cancel_ptr;

	Waiting(Stock &_stock, StockRequest &&_request,
		StockGetHandler &_handler,
		CancellablePointer &_cancel_ptr) noexcept;

	void Destroy() noexcept;

	/* virtual methods from class Cancellable */
	void Cancel() noexcept override;
};

inline
Stock::Waiting::Waiting(Stock &_stock, StockRequest &&_request,
			StockGetHandler &_handler,
			CancellablePointer &_cancel_ptr) noexcept
	:stock(_stock), request(std::move(_request)),
	 fairness_hash(_stock.cls.GetFairnessHash(request.get())),
	 handler(_handler),
	 cancel_ptr(_cancel_ptr)
{
	cancel_ptr = *this;
}

inline void
Stock::Waiting::Destroy() noexcept
{
	delete this;
}

void
Stock::DiscardUnused() noexcept
{
	if (clear_interval > Event::Duration::zero() && !may_clear)
		return;

	ClearIdle();

	may_clear = true;
	ScheduleClear();
	ScheduleCheckEmpty();
}

void
Stock::FadeAll() noexcept
{
	for (auto &i : busy)
		i.Fade();

	ClearIdle();
	ScheduleCheckEmpty();

	// TODO: restart the "num_create" list?
}

void
Stock::Shutdown() noexcept
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
Stock::CheckEmpty() noexcept
{
	if (IsEmpty() && handler != nullptr)
		handler->OnStockEmpty(*this);
}

void
Stock::ScheduleCheckEmpty() noexcept
{
	if (IsEmpty() && handler != nullptr)
		empty_event.Schedule();
}


/*
 * cleanup
 *
 */

void
Stock::CleanupEventCallback() noexcept
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
 * wait operation
 *
 */

void
Stock::Waiting::Cancel() noexcept
{
	auto &list = stock.waiting;
	const auto i = list.iterator_to(*this);
	list.erase_and_dispose(i, [](Stock::Waiting *w){ w->Destroy(); });
}

inline Stock::WaitingList::iterator
Stock::PickWaiting() noexcept
{
	if (last_fairness_hash == 0 || waiting.empty())
		/* fairness disabled or nobody is waiting */
		return waiting.begin();

	/* find the first "waiting" entry with a different
	   fairness_hash than the last one */
	const uint_fast64_t lfh = last_fairness_hash;
	for (auto &i : waiting)
		if (i.fairness_hash != lfh)
			return waiting.iterator_to(i);

	/* there is none: use an arbitrary waiting */
	return waiting.begin();
}

void
Stock::RetryWaiting() noexcept
{
	if (limit == 0)
		/* no limit configured, no waiters possible */
		return;

	/* first try to serve existing idle items */

	while (!idle.empty()) {
		const auto i = PickWaiting();
		if (i == waiting.end())
			return;

		auto &w = *i;

		last_fairness_hash = w.fairness_hash;

		waiting.erase(i);

		if (!GetIdle(w.request, w.handler)) {
			/* didn't work (probably because borrowing the item has
			   failed) - re-add to "waiting" list */
			waiting.push_front(w);
			break;
		}

		w.Destroy();
	}

	/* if we're below the limit, create a bunch of new items */

	for (std::size_t i = limit - GetActiveCount();
	     GetActiveCount() < limit && i > 0 && !waiting.empty();
	     --i) {
		auto &w = waiting.front();
		waiting.pop_front();

		GetCreate(std::move(w.request),
			  w.handler,
			  w.cancel_ptr);
		w.Destroy();
	}
}

void
Stock::ScheduleRetryWaiting() noexcept
{
	if (!waiting.empty() && !IsFull())
		retry_event.Schedule();
}


/*
 * clear after 60 seconds idle
 *
 */

void
Stock::ClearIdle() noexcept
{
	logger.Format(5, "ClearIdle num_idle=%zu num_busy=%zu",
		      idle.size(), busy.size());

	if (idle.size() > max_idle)
		UnscheduleCleanup();

	idle.clear_and_dispose(DeleteDisposer());
}

void
Stock::ClearEventCallback() noexcept
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

Stock::Stock(EventLoop &event_loop, StockClass &_cls,
	     const char *_name, std::size_t _limit, std::size_t _max_idle,
	     Event::Duration _clear_interval,
	     StockHandler *_handler) noexcept
	:cls(_cls),
	 name(_name),
	 limit(_limit), max_idle(_max_idle),
	 clear_interval(_clear_interval),
	 handler(_handler),
	 logger(name),
	 retry_event(event_loop, BIND_THIS_METHOD(RetryWaiting)),
	 empty_event(event_loop, BIND_THIS_METHOD(CheckEmpty)),
	 cleanup_event(event_loop, BIND_THIS_METHOD(CleanupEventCallback)),
	 clear_event(event_loop, BIND_THIS_METHOD(ClearEventCallback))
{
	assert(max_idle > 0);

	ScheduleClear();
}

Stock::~Stock() noexcept
{
	assert(num_create == 0);

	/* must not delete the Stock when there are busy items left */
	assert(busy.empty());

	retry_event.Cancel();
	empty_event.Cancel();
	cleanup_event.Cancel();
	clear_event.Cancel();

	ClearIdle();
}

StockItem *
Stock::GetIdle() noexcept
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
Stock::GetIdle(StockRequest &request,
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
Stock::GetCreate(StockRequest request,
		 StockGetHandler &get_handler,
		 CancellablePointer &cancel_ptr) noexcept
{
	++num_create;

	try {
		cls.Create({*this, get_handler},
			   std::move(request), cancel_ptr);
	} catch (...) {
		ItemCreateError(get_handler, std::current_exception());
	}
}

void
Stock::Get(StockRequest request,
	   StockGetHandler &get_handler,
	   CancellablePointer &cancel_ptr) noexcept
{
	may_clear = false;

	if (GetIdle(request, get_handler))
		return;

	if (IsFull()) {
		/* item limit reached: wait for an item to return */
		auto w = new Waiting(*this, std::move(request),
				     get_handler, cancel_ptr);
		waiting.push_front(*w);
		return;
	}

	GetCreate(std::move(request), get_handler, cancel_ptr);
}

StockItem *
Stock::GetNow(StockRequest request)
{
	struct NowRequest final : public StockGetHandler {
#ifndef NDEBUG
		bool created = false;
#endif
		StockItem *item;
		std::exception_ptr error;

		/* virtual methods from class StockGetHandler */
		void OnStockItemReady(StockItem &_item) noexcept override {
#ifndef NDEBUG
			created = true;
#endif

			item = &_item;
		}

		void OnStockItemError(std::exception_ptr ep) noexcept override {
#ifndef NDEBUG
			created = true;
#endif

			error = ep;
		}
	};

	NowRequest data;
	CancellablePointer cancel_ptr;

	/* cannot call this on a limited stock */
	assert(limit == 0);

	Get(std::move(request), data, cancel_ptr);
	assert(data.created);

	if (data.error)
		std::rethrow_exception(data.error);

	return data.item;
}

void
Stock::ItemCreateSuccess(StockItem &item) noexcept
{
	assert(num_create > 0);
	--num_create;

	busy.push_front(item);

	item.handler.OnStockItemReady(item);
}

void
Stock::ItemCreateError(StockGetHandler &get_handler,
		       std::exception_ptr ep) noexcept
{
	assert(num_create > 0);
	--num_create;

	ScheduleCheckEmpty();
	ScheduleRetryWaiting();

	get_handler.OnStockItemError(ep);
}

void
Stock::ItemCreateAborted() noexcept
{
	assert(num_create > 0);
	--num_create;

	ScheduleCheckEmpty();
	ScheduleRetryWaiting();
}

void
Stock::Put(StockItem &item, bool destroy) noexcept
{
	assert(!item.is_idle);
	assert(&item.stock == this);

	may_clear = false;

	assert(!busy.empty());

	busy.erase(busy.iterator_to(item));

	if (destroy || item.IsFading() || !item.Release()) {
		delete &item;
		ScheduleCheckEmpty();
		ScheduleRetryWaiting();
	} else {
		InjectIdle(item);
	}

	ScheduleRetryWaiting();
}

void
Stock::InjectIdle(StockItem &item) noexcept
{
	assert(!item.is_idle);
	assert(&item.stock == this);

#ifndef NDEBUG
	item.is_idle = true;
#endif

	if (idle.size() == max_idle)
		ScheduleCleanup();

	idle.push_front(item);

	ScheduleRetryWaiting();
}

void
Stock::ItemIdleDisconnect(StockItem &item) noexcept
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
Stock::ItemBusyDisconnect(StockItem &item) noexcept
{
	assert(!item.is_idle);

	/* this item will be destroyed by Put() */
	item.Fade();
}
