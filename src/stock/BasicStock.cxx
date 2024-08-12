// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "BasicStock.hxx"
#include "Class.hxx"
#include "GetHandler.hxx"
#include "util/Cancellable.hxx"

#include <cassert>

struct BasicStock::Create final
	: IntrusiveListHook<>, StockGetHandler, Cancellable
{
	BasicStock &stock;

	/**
	 * Request was canceled if this field is nullptr.
	 */
	StockGetHandler *handler;

	CancellablePointer cancel_ptr;

	const bool continue_on_cancel;

	Create(BasicStock &_stock,
	       bool _continue_on_cancel,
	       StockGetHandler &_handler, CancellablePointer &_cancel_ptr) noexcept
		:stock(_stock), handler(&_handler),
		 continue_on_cancel(_continue_on_cancel)
	{
		_cancel_ptr = *this;
	}

	bool IsDetached() const noexcept {
		return handler == nullptr;
	}

	void Detach() noexcept {
		assert(handler != nullptr);

		handler = nullptr;
	}

	void Attach(StockGetHandler &_handler, CancellablePointer &_cancel_ptr) noexcept {
		assert(handler == nullptr);

		handler = &_handler;
		_cancel_ptr = *this;
	}

	// virtual methods from class StockGetHandler
	[[noreturn]]
	void OnStockItemReady(StockItem &) noexcept override {
		// unreachable
		std::terminate();
	}

	[[noreturn]]
	void OnStockItemError(std::exception_ptr) noexcept override {
		// unreachable
		std::terminate();
	}

	// virtual methods from class Cancellable
	void Cancel() noexcept override {
		assert(handler != nullptr);

		stock.CreateCanceled(*this);
	}
};

void
BasicStock::FadeAll() noexcept
{
	for (auto &i : busy)
		i.Fade();

	ClearIdle();
	CheckEmpty();

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
	if (IsEmpty())
		OnEmpty();
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
	if (idle.size() > max_idle)
		UnscheduleCleanup();

	idle.clear_and_dispose(DeleteDisposer());
}

void
BasicStock::ClearEventCallback() noexcept
{
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
		       Event::Duration _clear_interval) noexcept
	:cls(_cls),
	 name(_name),
	 max_idle(_max_idle),
	 clear_interval(_clear_interval),
	 cleanup_event(event_loop, BIND_THIS_METHOD(CleanupEventCallback)),
	 clear_event(event_loop, BIND_THIS_METHOD(ClearEventCallback))
{
	assert(max_idle > 0);

	ScheduleClear();
}

BasicStock::~BasicStock() noexcept
{
	/* must not delete the Stock when there are busy items left */
	assert(busy.empty());

	ClearIdle();

	create.clear_and_dispose([](Create *c){
		/* by now, all create operations must have been
		   canceled */
		assert(c->handler == nullptr);
		assert(c->cancel_ptr);

		c->cancel_ptr.Cancel();
		delete c;
	});
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

	CheckEmpty();
	return nullptr;
}

bool
BasicStock::GetIdle(StockRequest &discard_request,
		    StockGetHandler &get_handler) noexcept
{
	auto *item = GetIdle();
	if (item == nullptr)
		return false;


	/* destroy the request before invoking the handler, because
	   the handler may destroy the memory pool, which may
	   invalidate the request's memory region */
	discard_request.reset();

	get_handler.OnStockItemReady(*item);
	return true;
}

bool
BasicStock::GetCanceled(StockGetHandler &get_handler,
			CancellablePointer &cancel_ptr) noexcept
{
	for (auto &c : create) {
		if (c.IsDetached()) {
			c.Attach(get_handler, cancel_ptr);
			return true;
		}
	}

	return false;
}

void
BasicStock::GetCreate(StockRequest request,
		      StockGetHandler &get_handler,
		      CancellablePointer &cancel_ptr) noexcept
{
	auto *c = new Create(*this,
			     cls.ShouldContinueOnCancel(request.get()),
			     get_handler, cancel_ptr);
	create.push_front(*c);

	try {
		cls.Create({*this}, std::move(request), *c, c->cancel_ptr);
	} catch (...) {
		ItemCreateError(*c, std::current_exception());
	}
}

void
BasicStock::ItemCreateSuccess(StockGetHandler &_handler,
			      StockItem &item) noexcept
{
	auto &c = static_cast<Create &>(_handler);
	auto *get_handler = c.handler;

	DeleteCreate(c);

	if (get_handler != nullptr) {
		busy.push_front(item);
		get_handler->OnStockItemReady(item);
	} else
		InjectIdle(item);
}

void
BasicStock::ItemCreateError(StockGetHandler &_handler,
			    std::exception_ptr ep) noexcept
{
	auto &c = static_cast<Create &>(_handler);
	auto *get_handler = c.handler;

	DeleteCreate(c);

	CheckEmpty();

	if (get_handler != nullptr)
		get_handler->OnStockItemError(ep);

	// TODO what to do with the error if the request was canceled?
}

inline void
BasicStock::CreateCanceled(Create &c) noexcept
{
	assert(c.cancel_ptr);

	if (c.continue_on_cancel) {
		// TOOD connect to waiting item?
		c.Detach();
	} else {
		c.cancel_ptr.Cancel();
		DeleteCreate(c);
		CheckEmpty();
	}

	OnCreateCanceled();
}

PutAction
BasicStock::Put(StockItem &item, PutAction action) noexcept
{
	assert(!item.is_idle);
	assert(&item.GetStock() == this);

	may_clear = false;

	assert(!busy.empty());

	busy.erase(busy.iterator_to(item));

	if (action == PutAction::DESTROY ||
	    item.IsFading() || !item.Release()) {
		delete &item;
		CheckEmpty();
		return PutAction::DESTROY;
	} else {
		InjectIdle(item);
		return PutAction::REUSE;
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
	CheckEmpty();
}

void
BasicStock::ItemBusyDisconnect(StockItem &item) noexcept
{
	assert(!item.is_idle);

	/* this item will be destroyed by Put() */
	item.Fade();
}


inline void
BasicStock::DeleteCreate(Create &c) noexcept
{
	assert(!create.empty());

	create.erase_and_dispose(create.iterator_to(c),
				 DeleteDisposer{});
}
