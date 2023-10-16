// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Stock.hxx"
#include "Class.hxx"
#include "GetHandler.hxx"
#include "util/Cancellable.hxx"

#include <cassert>

struct Stock::Waiting final
	: IntrusiveListHook<IntrusiveHookMode::NORMAL>, Cancellable
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

	while (HasIdle()) {
		const auto i = PickWaiting();
		if (i == waiting.end())
			return;

		auto &w = *i;

		last_fairness_hash = w.fairness_hash;

		waiting.erase(i);

		if (!GetIdle(w.request, w.handler)) {
			/* didn't work (probably because borrowing the item has
			   failed) - re-add to "waiting" list */
			waiting.push_back(w);
			break;
		}

		w.Destroy();
	}

	/* if we're below the limit, create a bunch of new items */

	for (std::size_t i = limit - GetActiveCount();
	     GetActiveCount() < limit && i > 0 && !waiting.empty();
	     --i) {
		auto &w = waiting.pop_front();

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
 * constructor
 *
 */

Stock::Stock(EventLoop &event_loop, StockClass &_cls,
	     const char *_name, std::size_t _limit, std::size_t _max_idle,
	     Event::Duration _clear_interval) noexcept
	:BasicStock(event_loop, _cls, _name,
		    _max_idle,
		    _clear_interval),
	 limit(_limit),
	 retry_event(event_loop, BIND_THIS_METHOD(RetryWaiting))
{
}

Stock::~Stock() noexcept = default;

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
		waiting.push_back(*w);
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
Stock::ItemCreateError(StockGetHandler &get_handler,
		       std::exception_ptr ep) noexcept
{
	BasicStock::ItemCreateError(get_handler, ep);
	ScheduleRetryWaiting();
}

void
Stock::ItemCreateAborted() noexcept
{
	BasicStock::ItemCreateAborted();
	ScheduleRetryWaiting();
}

PutAction
Stock::Put(StockItem &item, PutAction action) noexcept
{
	const auto result = BasicStock::Put(item, action);
	ScheduleRetryWaiting();
	return result;
}
