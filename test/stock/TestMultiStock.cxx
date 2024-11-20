// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "stock/MultiStock.hxx"
#include "stock/Class.hxx"
#include "event/Loop.hxx"

#include <gtest/gtest.h>

#include <forward_list>
#include <list>

namespace {

struct Partition;

struct MyStockItem final : StockItem {
	const StockRequest request;

	explicit MyStockItem(CreateStockItem c, StockRequest _request) noexcept
		:StockItem(c), request(std::move(_request)) {}

	~MyStockItem() noexcept override;

	/* virtual methods from class StockItem */
	bool Borrow() noexcept override {
		return true;
	}

	bool Release() noexcept override {
		return true;
	}
};

struct MyInnerStockItem final : StockItem {
	StockItem &outer_item;

	bool stopping = false;

	MyInnerStockItem(CreateStockItem c, StockItem &_outer_item) noexcept
		:StockItem(c), outer_item(_outer_item) {}

	/* virtual methods from class StockItem */
	bool Borrow() noexcept override {
		return true;
	}

	bool Release() noexcept override {
		unclean = std::exchange(stopping, false);
		return true;
	}
};

class MyStockClass final : public StockClass, public MultiStockClass {
	class DeferredRequest final : Cancellable {
		Partition &partition;

		CreateStockItem c;
		StockRequest request;
		StockGetHandler &handler;

		DeferEvent defer_event;

	public:
		DeferredRequest(Partition &_partition,
				CreateStockItem _c, StockRequest _request,
				StockGetHandler &_handler,
				CancellablePointer &cancel_ptr) noexcept;

	private:
		void OnDeferred() noexcept;
		void Cancel() noexcept override;
	};

	class NeverRequest final : Cancellable {
		StockRequest request;

	public:
		NeverRequest(StockRequest _request, CancellablePointer &cancel_ptr) noexcept
			:request(std::move(_request))
		{
			cancel_ptr = *this;
		}

	private:
		void Cancel() noexcept override {
			delete this;
		}
	};

public:
	/* virtual methods from class StockClass */
	void Create(CreateStockItem c, StockRequest request,
		    StockGetHandler &handler,
		    CancellablePointer &cancel_ptr) override;

	/* virtual methods from class MultiStockClass */
	std::size_t GetLimit(const void *,
			     std::size_t _limit) const noexcept override {
		return _limit;
	}

	Event::Duration GetClearInterval(const void *) const noexcept override {
		return std::chrono::hours{1};
	}

	StockItem *Create(CreateStockItem c, StockItem &outer_item) override {
		return new MyInnerStockItem(c, outer_item);
	}
};

struct Instance {
	EventLoop event_loop;

	DeferEvent break_event{event_loop, BIND_THIS_METHOD(OnBreakEvent)};

	MyStockClass stock_class;
	MultiStock multi_stock;

	explicit Instance(unsigned limit=1) noexcept
		:multi_stock(event_loop, stock_class, limit,
			     stock_class) {}

	void RunSome() noexcept {
		break_event.ScheduleIdle();
		event_loop.Run();
	}

private:
	void OnBreakEvent() noexcept {
		event_loop.Break();
	}
};

struct MyLease;

struct Partition {
	Instance &instance;
	const char *const key;

	std::size_t factory_created = 0, factory_failed = 0;
	std::size_t destroyed = 0;
	std::size_t total = 0, waiting = 0, ready = 0, failed = 0;

	std::list<MyLease> leases;

	/**
	 * This error will be produced by MyStockClass::Create().
	 */
	std::exception_ptr next_error;

	bool defer_create = false, never_create = false;

	Partition(Instance &_instance, const char *_key) noexcept
		:instance(_instance), key(_key) {}

	MyLease &Get() noexcept;

	void Get(std::size_t n) noexcept {
		for (std::size_t i = 0; i < n; ++i)
			Get();
	}

	void PutReady(unsigned n=256) noexcept;
	void PutDirty(unsigned n) noexcept;
	void PutOuterDirty() noexcept;
};

struct MyLease final : public StockGetHandler {
	Partition &partition;

	CancellablePointer get_cancel_ptr;

	MyInnerStockItem *item = nullptr;
	std::exception_ptr error;

	PutAction put_action = PutAction::REUSE;

	explicit MyLease(Partition &_partition) noexcept
		:partition(_partition)
	{
		++partition.total;
		++partition.waiting;
	}

	~MyLease() noexcept {
		assert(partition.total > 0);

		if (get_cancel_ptr) {
			assert(partition.waiting > 0);
			--partition.waiting;

			get_cancel_ptr.Cancel();
		} else if (item != nullptr)
			Release();

		--partition.total;
	}

	MyLease(const MyLease &) = delete;
	MyLease &operator=(const MyLease &) = delete;

	void SetDirty() noexcept {
		put_action = PutAction::DESTROY;
	}

	void Release() noexcept {
		assert(item != nullptr);
		assert(partition.total > 0);
		assert(partition.ready > 0);

		--partition.ready;

		if (put_action == PutAction::DESTROY)
			item->outer_item.Fade();
		item->Put(put_action);
		item = nullptr;
	}

	/* virtual methods from class StockGetHandler */
	void OnStockItemReady(StockItem &_item) noexcept override {
		assert(item == nullptr);
		assert(!error);
		assert(partition.total > 0);
		assert(partition.waiting > 0);

		get_cancel_ptr = nullptr;
		item = (MyInnerStockItem *)&_item;
		++partition.ready;
		--partition.waiting;
	}

	void OnStockItemError(std::exception_ptr _error) noexcept override {
		assert(item == nullptr);
		assert(!error);
		assert(partition.total > 0);
		assert(partition.waiting > 0);

		get_cancel_ptr = nullptr;
		error = std::move(_error);
		--partition.waiting;
		++partition.failed;
	}
};

MyLease &
Partition::Get() noexcept
{
	leases.emplace_back(*this);
	auto &lease = leases.back();
	instance.multi_stock.Get(key, ToNopPointer(this), 2,
				 lease, lease.get_cancel_ptr);
	return lease;
}

void
Partition::PutReady(unsigned n) noexcept
{
	for (auto i = leases.begin(), end = leases.end(); n > 0 && i != end;) {
		if (i->item != nullptr) {
			i = leases.erase(i);
			--n;
		} else
			++i;
	}
}

void
Partition::PutDirty(unsigned n) noexcept
{
	for (auto i = leases.begin(); n > 0;) {
		assert(i != leases.end());

		if (i->item != nullptr) {
			i->SetDirty();
			i = leases.erase(i);
			--n;
		} else
			++i;
	}

	// unreachable
}

void
Partition::PutOuterDirty() noexcept
{
	assert(!leases.empty());

	auto &item = leases.front();
	auto &outer_item = item.item->outer_item;
	leases.pop_front();

	outer_item.InvokeBusyDisconnect();
}

MyStockClass::DeferredRequest::DeferredRequest(Partition &_partition,
					       CreateStockItem _c,
					       StockRequest _request,
					       StockGetHandler &_handler,
					       CancellablePointer &cancel_ptr) noexcept
	:partition(_partition), c(_c), request(std::move(_request)),
	 handler(_handler),
	 defer_event(partition.instance.event_loop, BIND_THIS_METHOD(OnDeferred))
{
	cancel_ptr = *this;
	defer_event.Schedule();
}

void
MyStockClass::DeferredRequest::OnDeferred() noexcept
{
	if (partition.next_error) {
		++partition.factory_failed;

		c.InvokeCreateError(handler, partition.next_error);
	} else {
		++partition.factory_created;

		auto *item = new MyStockItem(c, std::move(request));
		item->InvokeCreateSuccess(handler);
	}

	delete this;
}

void
MyStockClass::DeferredRequest::Cancel() noexcept
{
	delete this;
}

void
MyStockClass::Create(CreateStockItem c,
		     StockRequest request,
		     StockGetHandler &handler,
		     CancellablePointer &cancel_ptr)
{
	auto &partition = *(Partition *)request.get();

	if (partition.defer_create) {
		new DeferredRequest(partition, c, std::move(request),
				    handler, cancel_ptr);
		return;
	}

	if (partition.never_create) {
		new NeverRequest(std::move(request), cancel_ptr);
		return;
	}

	if (partition.next_error) {
		++partition.factory_failed;

		c.InvokeCreateError(handler, partition.next_error);
	} else {
		++partition.factory_created;

		auto *item = new MyStockItem(c, std::move(request));
		item->InvokeCreateSuccess(handler);
	}
}

MyStockItem::~MyStockItem() noexcept
{
	auto &partition = *(Partition *)request.get();
	++partition.destroyed;
}

} // anonymous namespace

TEST(MultiStock, Basic)
{
	Instance instance;

	Partition foo{instance, "foo"};

	// request item, wait for it to be delivered

	foo.Get();
	instance.RunSome();

	ASSERT_EQ(foo.factory_created, 1);
	ASSERT_EQ(foo.destroyed, 0);
	ASSERT_EQ(foo.total, 1);
	ASSERT_EQ(foo.waiting, 0);
	ASSERT_EQ(foo.ready, 1);
	ASSERT_EQ(foo.failed, 0);

	// request 3 more items (2 more than is allowed)

	foo.Get();
	foo.Get();
	foo.Get();

	instance.RunSome();

	ASSERT_EQ(foo.factory_created, 1);
	ASSERT_EQ(foo.destroyed, 0);
	ASSERT_EQ(foo.total, 4);
	ASSERT_EQ(foo.waiting, 2);
	ASSERT_EQ(foo.ready, 2);
	ASSERT_EQ(foo.failed, 0);

	// release the first item; 1 waiting item will be handled, 1 remains waiting

	foo.leases.pop_front();

	instance.RunSome();

	ASSERT_EQ(foo.factory_created, 1);
	ASSERT_EQ(foo.destroyed, 0);
	ASSERT_EQ(foo.total, 3);
	ASSERT_EQ(foo.waiting, 1);
	ASSERT_EQ(foo.ready, 2);
	ASSERT_EQ(foo.failed, 0);

	// mark the item dirty (cannot be reused, 1 still waiting)

	foo.PutDirty(1);
	instance.RunSome();

	ASSERT_EQ(foo.factory_created, 1);
	ASSERT_EQ(foo.destroyed, 0);
	ASSERT_EQ(foo.total, 2);
	ASSERT_EQ(foo.waiting, 1);
	ASSERT_EQ(foo.ready, 1);
	ASSERT_EQ(foo.failed, 0);

	// release all other leases; a new item will be created

	foo.PutReady();

	instance.RunSome();

	ASSERT_EQ(foo.factory_created, 2);
	ASSERT_EQ(foo.destroyed, 1);
	ASSERT_EQ(foo.total, 1);
	ASSERT_EQ(foo.waiting, 0);
	ASSERT_EQ(foo.ready, 1);
	ASSERT_EQ(foo.failed, 0);
}

TEST(MultiStock, GetTooMany)
{
	Instance instance;

	Partition foo{instance, "foo"};

	/* request one more than allowed; this used to trigger an
	   assertion failure */
	foo.Get(3);

	instance.RunSome();

	ASSERT_EQ(foo.factory_created, 1);
	ASSERT_EQ(foo.factory_failed, 0);
	ASSERT_EQ(foo.destroyed, 0);
	ASSERT_EQ(foo.total, 3);
	ASSERT_EQ(foo.waiting, 1);
	ASSERT_EQ(foo.ready, 2);
	ASSERT_EQ(foo.failed, 0);

	foo.PutDirty(2);

	ASSERT_EQ(foo.factory_created, 1);
	ASSERT_EQ(foo.factory_failed, 0);
	ASSERT_EQ(foo.destroyed, 1);
	ASSERT_EQ(foo.total, 1);
	ASSERT_EQ(foo.waiting, 1);
	ASSERT_EQ(foo.ready, 0);
	ASSERT_EQ(foo.failed, 0);

	instance.RunSome();

	ASSERT_EQ(foo.factory_created, 2);
	ASSERT_EQ(foo.factory_failed, 0);
	ASSERT_EQ(foo.destroyed, 1);
	ASSERT_EQ(foo.total, 1);
	ASSERT_EQ(foo.waiting, 0);
	ASSERT_EQ(foo.ready, 1);
	ASSERT_EQ(foo.failed, 0);
}

TEST(MultiStock, DeferredCancel)
{
	Instance instance;

	Partition foo{instance, "foo"};
	foo.defer_create = true;

	foo.Get(16);

	ASSERT_EQ(foo.total, 16);
	ASSERT_EQ(foo.waiting, 16);
	ASSERT_EQ(foo.ready, 0);
	ASSERT_EQ(foo.failed, 0);

	foo.leases.clear();
	instance.RunSome();

	ASSERT_EQ(foo.total, 0);
	ASSERT_EQ(foo.waiting, 0);
	ASSERT_EQ(foo.ready, 0);
	ASSERT_EQ(foo.failed, 0);
}

TEST(MultiStock, DeferredWaitingCancel)
{
	Instance instance;

	Partition foo{instance, "foo"};
	foo.defer_create = true;

	foo.Get(16);

	ASSERT_EQ(foo.total, 16);
	ASSERT_EQ(foo.waiting, 16);
	ASSERT_EQ(foo.ready, 0);
	ASSERT_EQ(foo.failed, 0);

	instance.RunSome();

	ASSERT_EQ(foo.total, 16);
	ASSERT_EQ(foo.waiting, 14);
	ASSERT_EQ(foo.ready, 2);
	ASSERT_EQ(foo.failed, 0);

	foo.leases.clear();
	instance.RunSome();

	ASSERT_EQ(foo.total, 0);
	ASSERT_EQ(foo.waiting, 0);
	ASSERT_EQ(foo.ready, 0);
	ASSERT_EQ(foo.failed, 0);
}

TEST(MultiStock, Error)
{
	Instance instance;

	Partition foo{instance, "foo"};
	foo.next_error = std::make_exception_ptr(std::runtime_error{"Error"});

	foo.Get(16);

	ASSERT_EQ(foo.factory_created, 0);
	ASSERT_EQ(foo.factory_failed, 16);
	ASSERT_EQ(foo.destroyed, 0);
	ASSERT_EQ(foo.total, 16);
	ASSERT_EQ(foo.waiting, 0);
	ASSERT_EQ(foo.ready, 0);
	ASSERT_EQ(foo.failed, 16);
}

TEST(MultiStock, DeferredError)
{
	Instance instance;

	Partition foo{instance, "foo"};
	foo.defer_create = true;
	foo.next_error = std::make_exception_ptr(std::runtime_error{"Error"});

	foo.Get(16);

	ASSERT_EQ(foo.factory_created, 0);
	ASSERT_EQ(foo.factory_failed, 0);
	ASSERT_EQ(foo.destroyed, 0);
	ASSERT_EQ(foo.total, 16);
	ASSERT_EQ(foo.waiting, 16);
	ASSERT_EQ(foo.ready, 0);
	ASSERT_EQ(foo.failed, 0);

	instance.RunSome();

	ASSERT_EQ(foo.factory_created, 0);
	ASSERT_EQ(foo.factory_failed, 1);
	ASSERT_EQ(foo.destroyed, 0);
	ASSERT_EQ(foo.total, 16);
	ASSERT_EQ(foo.waiting, 0);
	ASSERT_EQ(foo.ready, 0);
	ASSERT_EQ(foo.failed, 16);
}

TEST(MultiStock, CreateTwo)
{
	Instance instance{2};

	Partition foo{instance, "foo"};
	foo.defer_create = true;

	foo.Get(16);

	ASSERT_EQ(foo.factory_created, 0);
	ASSERT_EQ(foo.factory_failed, 0);
	ASSERT_EQ(foo.destroyed, 0);
	ASSERT_EQ(foo.total, 16);
	ASSERT_EQ(foo.waiting, 16);
	ASSERT_EQ(foo.ready, 0);
	ASSERT_EQ(foo.failed, 0);

	instance.RunSome();

	ASSERT_EQ(foo.factory_created, 2);
	ASSERT_EQ(foo.factory_failed, 0);
	ASSERT_EQ(foo.destroyed, 0);
	ASSERT_EQ(foo.total, 16);
	ASSERT_EQ(foo.waiting, 12);
	ASSERT_EQ(foo.ready, 4);
	ASSERT_EQ(foo.failed, 0);

	foo.PutReady(1);
	instance.RunSome();

	ASSERT_EQ(foo.factory_created, 2);
	ASSERT_EQ(foo.factory_failed, 0);
	ASSERT_EQ(foo.destroyed, 0);
	ASSERT_EQ(foo.total, 15);
	ASSERT_EQ(foo.waiting, 11);
	ASSERT_EQ(foo.ready, 4);
	ASSERT_EQ(foo.failed, 0);

	foo.PutReady(4);
	instance.RunSome();

	ASSERT_EQ(foo.factory_created, 2);
	ASSERT_EQ(foo.factory_failed, 0);
	ASSERT_EQ(foo.destroyed, 0);
	ASSERT_EQ(foo.total, 11);
	ASSERT_EQ(foo.waiting, 7);
	ASSERT_EQ(foo.ready, 4);
	ASSERT_EQ(foo.failed, 0);

	foo.PutReady(4);
	instance.RunSome();

	ASSERT_EQ(foo.factory_created, 2);
	ASSERT_EQ(foo.factory_failed, 0);
	ASSERT_EQ(foo.destroyed, 0);
	ASSERT_EQ(foo.total, 7);
	ASSERT_EQ(foo.waiting, 3);
	ASSERT_EQ(foo.ready, 4);
	ASSERT_EQ(foo.failed, 0);

	foo.PutDirty(1);
	foo.PutReady(1);
	instance.RunSome();

	ASSERT_EQ(foo.factory_created, 3);
	ASSERT_EQ(foo.factory_failed, 0);
	ASSERT_EQ(foo.destroyed, 1);
	ASSERT_EQ(foo.total, 5);
	ASSERT_EQ(foo.waiting, 1);
	ASSERT_EQ(foo.ready, 4);
	ASSERT_EQ(foo.failed, 0);

	/* release all leases; one waiting request remains, but there
	   are two items; the MultiStock will assign one of them to
	   the waiting request, and will delete the other one */

	foo.PutReady();
	instance.RunSome();

	ASSERT_EQ(foo.factory_created, 3);
	ASSERT_EQ(foo.factory_failed, 0);
	ASSERT_EQ(foo.destroyed, 1);
	ASSERT_EQ(foo.total, 1);
	ASSERT_EQ(foo.waiting, 0);
	ASSERT_EQ(foo.ready, 1);
	ASSERT_EQ(foo.failed, 0);
}

TEST(MultiStock, FadeBusy)
{
	Instance instance;

	Partition foo{instance, "foo"};

	/* request one more than allowed */
	foo.Get(3);

	instance.RunSome();

	ASSERT_EQ(foo.factory_created, 1);
	ASSERT_EQ(foo.factory_failed, 0);
	ASSERT_EQ(foo.destroyed, 0);
	ASSERT_EQ(foo.total, 3);
	ASSERT_EQ(foo.waiting, 1);
	ASSERT_EQ(foo.ready, 2);
	ASSERT_EQ(foo.failed, 0);

	/* enable "fade"; this means no change right now, because no
	   item is removed */
	instance.multi_stock.FadeAll();
	instance.RunSome();

	ASSERT_EQ(foo.factory_created, 1);
	ASSERT_EQ(foo.factory_failed, 0);
	ASSERT_EQ(foo.destroyed, 0);
	ASSERT_EQ(foo.total, 3);
	ASSERT_EQ(foo.waiting, 1);
	ASSERT_EQ(foo.ready, 2);
	ASSERT_EQ(foo.failed, 0);

	/* release one; the waiting client won't be handled because
	   the one item is in "fade" mode */
	foo.PutReady(1);
	instance.RunSome();

	ASSERT_EQ(foo.factory_created, 1);
	ASSERT_EQ(foo.factory_failed, 0);
	ASSERT_EQ(foo.destroyed, 0);
	ASSERT_EQ(foo.total, 2);
	ASSERT_EQ(foo.waiting, 1);
	ASSERT_EQ(foo.ready, 1);
	ASSERT_EQ(foo.failed, 0);

	/* release the last one; now the existing item will be
	   destroyed and a new one is created */
	foo.PutReady(1);
	instance.RunSome();

	ASSERT_EQ(foo.factory_created, 2);
	ASSERT_EQ(foo.factory_failed, 0);
	ASSERT_EQ(foo.destroyed, 1);
	ASSERT_EQ(foo.total, 1);
	ASSERT_EQ(foo.waiting, 0);
	ASSERT_EQ(foo.ready, 1);
	ASSERT_EQ(foo.failed, 0);
}

TEST(MultiStock, FadeIdle)
{
	Instance instance;

	Partition foo{instance, "foo"};

	/* create one */
	foo.Get(1);
	instance.RunSome();
	ASSERT_EQ(foo.factory_created, 1);
	ASSERT_EQ(foo.factory_failed, 0);
	ASSERT_EQ(foo.destroyed, 0);
	ASSERT_EQ(foo.total, 1);
	ASSERT_EQ(foo.waiting, 0);
	ASSERT_EQ(foo.ready, 1);
	ASSERT_EQ(foo.failed, 0);

	/* release it; it will remain idle */
	foo.PutReady(1);
	instance.RunSome();
	ASSERT_EQ(foo.factory_created, 1);
	ASSERT_EQ(foo.factory_failed, 0);
	ASSERT_EQ(foo.destroyed, 0);
	ASSERT_EQ(foo.total, 0);
	ASSERT_EQ(foo.waiting, 0);
	ASSERT_EQ(foo.ready, 0);
	ASSERT_EQ(foo.failed, 0);

	/* fade it; the one idle item is destroyed now */
	instance.multi_stock.FadeAll();
	instance.RunSome();
	ASSERT_EQ(foo.factory_created, 1);
	ASSERT_EQ(foo.factory_failed, 0);
	ASSERT_EQ(foo.destroyed, 1);
	ASSERT_EQ(foo.total, 0);
	ASSERT_EQ(foo.waiting, 0);
	ASSERT_EQ(foo.ready, 0);
	ASSERT_EQ(foo.failed, 0);

	/* request a new item */
	foo.Get(1);
	instance.RunSome();
	ASSERT_EQ(foo.factory_created, 2);
	ASSERT_EQ(foo.factory_failed, 0);
	ASSERT_EQ(foo.destroyed, 1);
	ASSERT_EQ(foo.total, 1);
	ASSERT_EQ(foo.waiting, 0);
	ASSERT_EQ(foo.ready, 1);
	ASSERT_EQ(foo.failed, 0);
}

TEST(MultiStock, FadeOuter)
{
	Instance instance;

	Partition foo{instance, "foo"};

	/* create one */
	foo.Get(1);
	instance.RunSome();
	ASSERT_EQ(foo.factory_created, 1);
	ASSERT_EQ(foo.factory_failed, 0);
	ASSERT_EQ(foo.destroyed, 0);
	ASSERT_EQ(foo.total, 1);
	ASSERT_EQ(foo.waiting, 0);
	ASSERT_EQ(foo.ready, 1);
	ASSERT_EQ(foo.failed, 0);

	/* release it, fade the outer item */
	foo.PutOuterDirty();
	instance.RunSome();
	ASSERT_EQ(foo.factory_created, 1);
	ASSERT_EQ(foo.factory_failed, 0);
	ASSERT_EQ(foo.destroyed, 1);
	ASSERT_EQ(foo.total, 0);
	ASSERT_EQ(foo.waiting, 0);
	ASSERT_EQ(foo.ready, 0);
	ASSERT_EQ(foo.failed, 0);

	/* request a new item */
	foo.Get(1);
	instance.RunSome();
	ASSERT_EQ(foo.factory_created, 2);
	ASSERT_EQ(foo.factory_failed, 0);
	ASSERT_EQ(foo.destroyed, 1);
	ASSERT_EQ(foo.total, 1);
	ASSERT_EQ(foo.waiting, 0);
	ASSERT_EQ(foo.ready, 1);
	ASSERT_EQ(foo.failed, 0);
}

TEST(MultiStock, ConsumedRequest)
{
	Instance instance{2};

	Partition foo{instance, "foo"};

	/* create 6 (4 ready and 2 waiting) */
	foo.Get(6);
	instance.RunSome();
	ASSERT_EQ(foo.factory_created, 2);
	ASSERT_EQ(foo.factory_failed, 0);
	ASSERT_EQ(foo.destroyed, 0);
	ASSERT_EQ(foo.total, 6);
	ASSERT_EQ(foo.waiting, 2);
	ASSERT_EQ(foo.ready, 4);
	ASSERT_EQ(foo.failed, 0);

	/* release 2 */
	foo.PutDirty(2);
	ASSERT_EQ(foo.factory_created, 2);
	ASSERT_EQ(foo.factory_failed, 0);
	ASSERT_EQ(foo.destroyed, 1);
	ASSERT_EQ(foo.total, 4);
	ASSERT_EQ(foo.waiting, 2);
	ASSERT_EQ(foo.ready, 2);
	ASSERT_EQ(foo.failed, 0);

	/* create a new one */
	/* this triggers a bug: the "request" object is consumed, but
	   the item will be used by the old "waiting" list, causing an
	   assertion failure when another item needs to be created,
	   but the request object is gone */
	foo.Get(1);
	instance.RunSome();
	ASSERT_EQ(foo.factory_created, 3);
	ASSERT_EQ(foo.factory_failed, 0);
	ASSERT_EQ(foo.destroyed, 1);
	ASSERT_EQ(foo.total, 5);
	ASSERT_EQ(foo.waiting, 1);
	ASSERT_EQ(foo.ready, 4);
	ASSERT_EQ(foo.failed, 0);
}

TEST(MultiStock, DiscardOldestIdle)
{
	Instance instance{4};

	Partition foo{instance, "foo"};
	Partition bar{instance, "bar"};

	foo.Get(8);
	bar.Get(8);
	ASSERT_EQ(instance.multi_stock.DiscardOldestIdle(1000), 0U);

	ASSERT_EQ(foo.factory_created, 4);
	ASSERT_EQ(foo.factory_failed, 0);
	ASSERT_EQ(foo.destroyed, 0);
	ASSERT_EQ(foo.total, 8);
	ASSERT_EQ(foo.waiting, 0);
	ASSERT_EQ(foo.ready, 8);
	ASSERT_EQ(foo.failed, 0);

	ASSERT_EQ(bar.factory_created, 4);
	ASSERT_EQ(bar.factory_failed, 0);
	ASSERT_EQ(bar.destroyed, 0);
	ASSERT_EQ(bar.total, 8);
	ASSERT_EQ(bar.waiting, 0);
	ASSERT_EQ(bar.ready, 8);
	ASSERT_EQ(bar.failed, 0);

	ASSERT_EQ(instance.multi_stock.DiscardOldestIdle(1), 0U);

	foo.PutReady(4);
	bar.PutReady(4);

	ASSERT_EQ(foo.factory_created, 4);
	ASSERT_EQ(foo.factory_failed, 0);
	ASSERT_EQ(foo.destroyed, 0);
	ASSERT_EQ(foo.total, 4);
	ASSERT_EQ(foo.waiting, 0);
	ASSERT_EQ(foo.ready, 4);
	ASSERT_EQ(foo.failed, 0);

	ASSERT_EQ(bar.factory_created, 4);
	ASSERT_EQ(bar.factory_failed, 0);
	ASSERT_EQ(bar.destroyed, 0);
	ASSERT_EQ(bar.total, 4);
	ASSERT_EQ(bar.waiting, 0);
	ASSERT_EQ(bar.ready, 4);
	ASSERT_EQ(bar.failed, 0);

	ASSERT_EQ(instance.multi_stock.DiscardOldestIdle(1), 2U);

	ASSERT_EQ(foo.factory_created, 4);
	ASSERT_EQ(foo.factory_failed, 0);
	ASSERT_EQ(foo.destroyed, 2);
	ASSERT_EQ(foo.total, 4);
	ASSERT_EQ(foo.waiting, 0);
	ASSERT_EQ(foo.ready, 4);
	ASSERT_EQ(foo.failed, 0);

	ASSERT_EQ(bar.factory_created, 4);
	ASSERT_EQ(bar.factory_failed, 0);
	ASSERT_EQ(bar.destroyed, 0);
	ASSERT_EQ(bar.total, 4);
	ASSERT_EQ(bar.waiting, 0);
	ASSERT_EQ(bar.ready, 4);
	ASSERT_EQ(bar.failed, 0);

	ASSERT_EQ(instance.multi_stock.DiscardOldestIdle(1000), 2U);

	ASSERT_EQ(foo.factory_created, 4);
	ASSERT_EQ(foo.factory_failed, 0);
	ASSERT_EQ(foo.destroyed, 2);
	ASSERT_EQ(foo.total, 4);
	ASSERT_EQ(foo.waiting, 0);
	ASSERT_EQ(foo.ready, 4);
	ASSERT_EQ(foo.failed, 0);

	ASSERT_EQ(bar.factory_created, 4);
	ASSERT_EQ(bar.factory_failed, 0);
	ASSERT_EQ(bar.destroyed, 2);
	ASSERT_EQ(bar.total, 4);
	ASSERT_EQ(bar.waiting, 0);
	ASSERT_EQ(bar.ready, 4);
	ASSERT_EQ(bar.failed, 0);

	ASSERT_EQ(instance.multi_stock.DiscardOldestIdle(1000), 0U);

	/* discard more than one */

	foo.PutReady(4);
	bar.PutReady(4);

	ASSERT_EQ(foo.factory_created, 4);
	ASSERT_EQ(foo.factory_failed, 0);
	ASSERT_EQ(foo.destroyed, 2);
	ASSERT_EQ(foo.total, 0);
	ASSERT_EQ(foo.waiting, 0);
	ASSERT_EQ(foo.ready, 0);
	ASSERT_EQ(foo.failed, 0);

	ASSERT_EQ(bar.factory_created, 4);
	ASSERT_EQ(bar.factory_failed, 0);
	ASSERT_EQ(bar.destroyed, 2);
	ASSERT_EQ(bar.total, 0);
	ASSERT_EQ(bar.waiting, 0);
	ASSERT_EQ(bar.ready, 0);
	ASSERT_EQ(bar.failed, 0);

	ASSERT_EQ(instance.multi_stock.DiscardOldestIdle(1000), 4U);

	ASSERT_EQ(foo.factory_created, 4);
	ASSERT_EQ(foo.factory_failed, 0);
	ASSERT_EQ(foo.destroyed, 4);
	ASSERT_EQ(foo.total, 0);
	ASSERT_EQ(foo.waiting, 0);
	ASSERT_EQ(foo.ready, 0);
	ASSERT_EQ(foo.failed, 0);

	ASSERT_EQ(bar.factory_created, 4);
	ASSERT_EQ(bar.factory_failed, 0);
	ASSERT_EQ(bar.destroyed, 4);
	ASSERT_EQ(bar.total, 0);
	ASSERT_EQ(bar.waiting, 0);
	ASSERT_EQ(bar.ready, 0);
	ASSERT_EQ(bar.failed, 0);
}

TEST(MultiStock, TriggerDoubleCreateBug)
{
	Instance instance{2};

	Partition foo{instance, "foo"};

	/* create four leases for two "outer" items */
	foo.Get(4);
	instance.RunSome();

	EXPECT_EQ(foo.factory_created, 2);
	EXPECT_EQ(foo.factory_failed, 0);
	EXPECT_EQ(foo.destroyed, 0);
	EXPECT_EQ(foo.total, 4);
	EXPECT_EQ(foo.waiting, 0);
	EXPECT_EQ(foo.ready, 4);
	EXPECT_EQ(foo.failed, 0);

	/* request another item (waiting) */
	foo.never_create = true;
	foo.Get(1);
	instance.RunSome();

	EXPECT_EQ(foo.factory_created, 2);
	EXPECT_EQ(foo.factory_failed, 0);
	EXPECT_EQ(foo.destroyed, 0);
	EXPECT_EQ(foo.total, 5);
	EXPECT_EQ(foo.waiting, 1);
	EXPECT_EQ(foo.ready, 4);
	EXPECT_EQ(foo.failed, 0);

	/* release one "outer" item, triggering a "create" for the
           waiter */

	foo.PutOuterDirty();
	foo.PutOuterDirty();
	foo.PutOuterDirty();
	instance.RunSome();

	/* release the second "outer" item, which used to trigger
           another "create" for the waiter */

	foo.PutOuterDirty();
	instance.RunSome();

	EXPECT_EQ(foo.factory_created, 2);
	EXPECT_EQ(foo.factory_failed, 0);
	EXPECT_EQ(foo.destroyed, 2);
	EXPECT_EQ(foo.total, 1);
	EXPECT_EQ(foo.waiting, 1);
	EXPECT_EQ(foo.ready, 0);
	EXPECT_EQ(foo.failed, 0);
}

/**
 * Unit test to verify that #MultiStock obeys the concurrency limit
 * even in the presence of "unclean" idle items.
 */
TEST(MultiStock, Unclean)
{
	Instance instance;

	Partition foo{instance, "foo"};

	foo.Get(5);

	instance.RunSome();

	ASSERT_EQ(foo.factory_created, 1);
	ASSERT_EQ(foo.factory_failed, 0);
	ASSERT_EQ(foo.destroyed, 0);
	ASSERT_EQ(foo.total, 5);
	ASSERT_EQ(foo.waiting, 3);
	ASSERT_EQ(foo.ready, 2);
	ASSERT_EQ(foo.failed, 0);

	/* make all inner items "unclean"; returning it will not allow
           it to be reused (yet) */
	std::forward_list<MyInnerStockItem *> unclean;
	for (auto &i : foo.leases) {
		if (i.item != nullptr) {
			i.item->stopping = true;
			unclean.push_front(i.item);
		}
	}

	ASSERT_EQ(std::distance(unclean.begin(), unclean.end()), 2U);

	foo.PutReady(2);

	ASSERT_EQ(foo.factory_created, 1);
	ASSERT_EQ(foo.factory_failed, 0);
	ASSERT_EQ(foo.destroyed, 0);
	ASSERT_EQ(foo.total, 3);
	ASSERT_EQ(foo.waiting, 3);
	ASSERT_EQ(foo.ready, 0);
	ASSERT_EQ(foo.failed, 0);

	instance.RunSome();

	ASSERT_EQ(foo.factory_created, 1);
	ASSERT_EQ(foo.factory_failed, 0);
	ASSERT_EQ(foo.destroyed, 0);
	ASSERT_EQ(foo.total, 3);
	ASSERT_EQ(foo.waiting, 3);
	ASSERT_EQ(foo.ready, 0);
	ASSERT_EQ(foo.failed, 0);

	/* attempt to get another lease */

	foo.Get(1);
	instance.RunSome();

	ASSERT_EQ(foo.factory_created, 1);
	ASSERT_EQ(foo.factory_failed, 0);
	ASSERT_EQ(foo.destroyed, 0);
	ASSERT_EQ(foo.total, 4);
	ASSERT_EQ(foo.waiting, 4);
	ASSERT_EQ(foo.ready, 0);
	ASSERT_EQ(foo.failed, 0);

	/* clear the "unclean" flags; this should allow the remaining
	   requests to be completed */

	auto &unclean1 = *unclean.front();
	unclean.pop_front();

	unclean1.ClearUncleanFlag();
	instance.RunSome();

	ASSERT_EQ(foo.factory_created, 1);
	ASSERT_EQ(foo.factory_failed, 0);
	ASSERT_EQ(foo.destroyed, 0);
	ASSERT_EQ(foo.total, 4);
	ASSERT_EQ(foo.waiting, 3);
	ASSERT_EQ(foo.ready, 1);
	ASSERT_EQ(foo.failed, 0);

	/* destroy the second unclean item; this will allow creating a
           new inner item */

	auto &unclean2 = *unclean.front();
	unclean.pop_front();
	ASSERT_TRUE(unclean.empty());

	unclean2.InvokeIdleDisconnect();
	instance.RunSome();

	ASSERT_EQ(foo.factory_created, 1);
	ASSERT_EQ(foo.factory_failed, 0);
	ASSERT_EQ(foo.destroyed, 0);
	ASSERT_EQ(foo.total, 4);
	ASSERT_EQ(foo.waiting, 2);
	ASSERT_EQ(foo.ready, 2);
	ASSERT_EQ(foo.failed, 0);

	/* return all items, flushing the "waiting" list */

	foo.PutReady(1);
	instance.RunSome();

	ASSERT_EQ(foo.factory_created, 1);
	ASSERT_EQ(foo.factory_failed, 0);
	ASSERT_EQ(foo.destroyed, 0);
	ASSERT_EQ(foo.total, 3);
	ASSERT_EQ(foo.waiting, 1);
	ASSERT_EQ(foo.ready, 2);
	ASSERT_EQ(foo.failed, 0);

	foo.PutReady(1);
	instance.RunSome();

	ASSERT_EQ(foo.factory_created, 1);
	ASSERT_EQ(foo.factory_failed, 0);
	ASSERT_EQ(foo.destroyed, 0);
	ASSERT_EQ(foo.total, 2);
	ASSERT_EQ(foo.waiting, 0);
	ASSERT_EQ(foo.ready, 2);
	ASSERT_EQ(foo.failed, 0);

	foo.PutReady(2);
	instance.RunSome();

	ASSERT_EQ(foo.factory_created, 1);
	ASSERT_EQ(foo.factory_failed, 0);
	ASSERT_EQ(foo.destroyed, 0);
	ASSERT_EQ(foo.total, 0);
	ASSERT_EQ(foo.waiting, 0);
	ASSERT_EQ(foo.ready, 0);
	ASSERT_EQ(foo.failed, 0);
}

/**
 * Regression test for a specific MultiStock bug that led to a stalled
 * "waiting" list when ScheduleRetryWaiting() gets intercepted by
 * DiscardOldestIdle().
 */
TEST(MultiStock, UncleanDiscardOldestIdleBug)
{
	Instance instance;

	Partition foo{instance, "foo"};

	/* attempt to get 3 leases - one more than the limit of 2 */
	foo.Get(3);

	instance.RunSome();

	ASSERT_EQ(foo.factory_created, 1);
	ASSERT_EQ(foo.factory_failed, 0);
	ASSERT_EQ(foo.destroyed, 0);
	ASSERT_EQ(foo.total, 3);
	ASSERT_EQ(foo.waiting, 1);
	ASSERT_EQ(foo.ready, 2);
	ASSERT_EQ(foo.failed, 0);

	/* make all inner items "unclean"; returning it will not allow
           it to be reused (yet) */
	std::forward_list<MyInnerStockItem *> unclean;
	for (auto &i : foo.leases) {
		if (i.item != nullptr) {
			i.item->stopping = true;
			unclean.push_front(i.item);
		}
	}

	ASSERT_EQ(std::distance(unclean.begin(), unclean.end()), 2U);

	foo.PutReady();
	instance.RunSome();

	ASSERT_EQ(foo.factory_created, 1);
	ASSERT_EQ(foo.factory_failed, 0);
	ASSERT_EQ(foo.destroyed, 0);
	ASSERT_EQ(foo.total, 1);
	ASSERT_EQ(foo.waiting, 1);
	ASSERT_EQ(foo.ready, 0);
	ASSERT_EQ(foo.failed, 0);

	/* this call used to discard the "unclean" item that was being
	   waited on; that means the "waiting" list was never again
	   retried because retry_event was never scheduled */
	instance.multi_stock.DiscardOldestIdle(2);

	ASSERT_EQ(foo.factory_created, 1);
	ASSERT_EQ(foo.factory_failed, 0);
	ASSERT_EQ(foo.destroyed, 0);
	ASSERT_EQ(foo.total, 1);
	ASSERT_EQ(foo.waiting, 1);
	ASSERT_EQ(foo.ready, 0);
	ASSERT_EQ(foo.failed, 0);

	instance.RunSome();

	ASSERT_EQ(foo.factory_created, 1);
	ASSERT_EQ(foo.factory_failed, 0);
	ASSERT_EQ(foo.destroyed, 0);
	ASSERT_EQ(foo.total, 1);
	ASSERT_EQ(foo.waiting, 1);
	ASSERT_EQ(foo.ready, 0);
	ASSERT_EQ(foo.failed, 0);

	/* clear the unclean flag of one item; this will finally allow
	   it to be reused */
	auto &unclean1 = *unclean.front();
	unclean.pop_front();

	unclean1.ClearUncleanFlag();
	instance.RunSome();

	ASSERT_EQ(foo.factory_created, 1);
	ASSERT_EQ(foo.factory_failed, 0);
	ASSERT_EQ(foo.destroyed, 0);
	ASSERT_EQ(foo.total, 1);
	ASSERT_EQ(foo.waiting, 0);
	ASSERT_EQ(foo.ready, 1);
	ASSERT_EQ(foo.failed, 0);
}
