// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "stock/MapStock.hxx"
#include "stock/Class.hxx"
#include "stock/GetHandler.hxx"
#include "stock/Item.hxx"
#include "stock/Options.hxx"
#include "event/Loop.hxx"
#include "util/Cancellable.hxx"
#include "util/PrintException.hxx"

#include <gtest/gtest.h>

#include <stdexcept>

#include <assert.h>

namespace {

static unsigned num_borrow, num_release, num_destroy;

struct MyStockItem final : StockItem {
	StockRequest request;

	explicit MyStockItem(CreateStockItem c)
		:StockItem(c) {}

	~MyStockItem() override {
		++num_destroy;
	}

	/* virtual methods from class StockItem */
	bool Borrow() noexcept override {
		++num_borrow;
		return true;
	}

	bool Release() noexcept override {
		++num_release;
		return true;
	}
};

class MyStockClass final : public StockClass {
public:
	unsigned n_create = 0, n_fail = 0;

	bool next_fail = false;

	/* virtual methods from class StockClass */
	void Create(CreateStockItem c, StockRequest request,
		    StockGetHandler &handler,
		    CancellablePointer &cancel_ptr) override;
};

void
MyStockClass::Create(CreateStockItem c,
		     StockRequest request,
		     StockGetHandler &handler,
		     CancellablePointer &)
{
	auto *item = new MyStockItem(c);
	item->request = std::move(request);

	if (next_fail) {
		++n_fail;
		delete item;
		throw std::runtime_error("next_fail");
	} else {
		++n_create;
		item->InvokeCreateSuccess(handler);
	}
}

class MyStockGetHandler final : public StockGetHandler {
public:
	bool got_item = false;
	StockItem *last_item = nullptr;

	/* virtual methods from class StockGetHandler */
	void OnStockItemReady(StockItem &item) noexcept override {
		assert(!got_item);

		got_item = true;
		last_item = &item;
	}

	void OnStockItemError(std::exception_ptr ep) noexcept override {
		PrintException(ep);

		got_item = true;
		last_item = nullptr;
	}
};

struct Instance {
	EventLoop event_loop;

	DeferEvent break_event{event_loop, BIND_THIS_METHOD(OnBreakEvent)};

	void RunSome() noexcept {
		break_event.ScheduleIdle();
		event_loop.Run();
	}

private:
	void OnBreakEvent() noexcept {
		event_loop.Break();
	}
};

} // anonymous namespace

/**
 * Test basic StockMap operations: different keys produce different
 * items, and the same key reuses an idle item.
 */
TEST(StockMap, Basic)
{
	Instance instance;
	MyStockClass cls;
	StockMap map{
		instance.event_loop, cls,
		{
			.limit = 3,
			.max_idle = 8,
		},
	};

	MyStockGetHandler handler;
	CancellablePointer cancel_ptr;

	num_borrow = num_release = num_destroy = 0;

	/* create first item under key "a" */

	map.Get(StockKey{"a"}, nullptr, handler, cancel_ptr);
	ASSERT_TRUE(handler.got_item);
	ASSERT_NE(handler.last_item, nullptr);
	ASSERT_EQ(cls.n_create, 1);
	ASSERT_EQ(num_borrow, 0);
	ASSERT_EQ(num_destroy, 0);
	auto *item_a = handler.last_item;

	/* release it */

	item_a->Put(PutAction::REUSE);
	instance.RunSome();
	ASSERT_EQ(num_release, 1);
	ASSERT_EQ(num_destroy, 0);

	/* get from key "a" again - should reuse the same item */

	handler.got_item = false;
	handler.last_item = nullptr;
	map.Get(StockKey{"a"}, nullptr, handler, cancel_ptr);
	ASSERT_TRUE(handler.got_item);
	ASSERT_EQ(handler.last_item, item_a);
	ASSERT_EQ(cls.n_create, 1);
	ASSERT_EQ(num_borrow, 1);

	/* get from key "b" - should create a new item */

	handler.got_item = false;
	handler.last_item = nullptr;
	map.Get(StockKey{"b"}, nullptr, handler, cancel_ptr);
	ASSERT_TRUE(handler.got_item);
	ASSERT_NE(handler.last_item, nullptr);
	ASSERT_NE(handler.last_item, item_a);
	ASSERT_EQ(cls.n_create, 2);
	auto *item_b = handler.last_item;

	/* clean up */

	item_a->Put(PutAction::DESTROY);
	item_b->Put(PutAction::DESTROY);
	ASSERT_EQ(num_destroy, 2);
}

/**
 * Test GetNow() for synchronous item creation and reuse.
 */
TEST(StockMap, GetNow)
{
	Instance instance;
	MyStockClass cls;
	StockMap map{
		instance.event_loop, cls,
		{
			.max_idle = 8,
		},
	};

	num_borrow = num_release = num_destroy = 0;

	/* create item under key "a" synchronously */

	auto *item_a = map.GetNow(StockKey{"a"}, nullptr);
	ASSERT_NE(item_a, nullptr);
	ASSERT_EQ(cls.n_create, 1);

	/* create item under key "b" */

	auto *item_b = map.GetNow(StockKey{"b"}, nullptr);
	ASSERT_NE(item_b, nullptr);
	ASSERT_NE(item_b, item_a);
	ASSERT_EQ(cls.n_create, 2);

	/* release item_a and get again - should reuse */

	item_a->Put(PutAction::REUSE);
	instance.RunSome();
	ASSERT_EQ(num_release, 1);
	ASSERT_EQ(num_destroy, 0);

	auto *item_a2 = map.GetNow(StockKey{"a"}, nullptr);
	ASSERT_EQ(item_a2, item_a);
	ASSERT_EQ(cls.n_create, 2);
	ASSERT_EQ(num_borrow, 1);

	/* clean up */

	item_a2->Put(PutAction::DESTROY);
	item_b->Put(PutAction::DESTROY);
	ASSERT_EQ(num_destroy, 2);
}

/**
 * Test FadeAll(): all idle items across all keys are destroyed.
 */
TEST(StockMap, FadeAll)
{
	Instance instance;
	MyStockClass cls;
	StockMap map{
		instance.event_loop, cls,
		{
			.limit = 3,
			.max_idle = 8,
		},
	};

	MyStockGetHandler handler;
	CancellablePointer cancel_ptr;

	num_borrow = num_release = num_destroy = 0;

	/* create items under two keys */

	map.Get(StockKey{"a"}, nullptr, handler, cancel_ptr);
	auto *item_a = handler.last_item;

	handler.got_item = false;
	handler.last_item = nullptr;
	map.Get(StockKey{"b"}, nullptr, handler, cancel_ptr);
	auto *item_b = handler.last_item;

	ASSERT_EQ(cls.n_create, 2);

	/* release both so they become idle */

	item_a->Put(PutAction::REUSE);
	item_b->Put(PutAction::REUSE);
	instance.RunSome();
	ASSERT_EQ(num_release, 2);
	ASSERT_EQ(num_destroy, 0);

	/* fade all - idle items are destroyed */

	map.FadeAll();
	ASSERT_EQ(num_destroy, 2);

	instance.RunSome();

	/* get from key "a" again - should create a fresh item */

	handler.got_item = false;
	handler.last_item = nullptr;
	map.Get(StockKey{"a"}, nullptr, handler, cancel_ptr);
	ASSERT_TRUE(handler.got_item);
	ASSERT_NE(handler.last_item, nullptr);
	ASSERT_EQ(cls.n_create, 3);

	handler.last_item->Put(PutAction::DESTROY);
}

/**
 * Test FadeKey() with a key that does not exist in the map.
 * Should be a no-op.
 */
TEST(StockMap, FadeKeyNonexistent)
{
	Instance instance;
	MyStockClass cls;
	StockMap map{
		instance.event_loop, cls,
		{
			.limit = 3,
			.max_idle = 8,
		},
	};

	MyStockGetHandler handler;
	CancellablePointer cancel_ptr;

	num_borrow = num_release = num_destroy = 0;

	/* create one item under "a" */

	map.Get(StockKey{"a"}, nullptr, handler, cancel_ptr);
	ASSERT_TRUE(handler.got_item);
	auto *item_a = handler.last_item;

	/* fade a key that doesn't exist - should be a no-op */

	map.FadeKey(StockKey{"nonexistent"});
	instance.RunSome();

	ASSERT_EQ(cls.n_create, 1);
	ASSERT_EQ(num_destroy, 0);

	item_a->Put(PutAction::DESTROY);
}

/**
 * Test FadeKey() on idle items: only the targeted key's items are
 * destroyed, items under other keys remain.
 */
TEST(StockMap, FadeKeyIdle)
{
	Instance instance;
	MyStockClass cls;
	StockMap map{
		instance.event_loop, cls,
		{
			.limit = 3,
			.max_idle = 8,
		},
	};

	MyStockGetHandler handler;
	CancellablePointer cancel_ptr;

	num_borrow = num_release = num_destroy = 0;

	/* create items under two keys */

	map.Get(StockKey{"a"}, nullptr, handler, cancel_ptr);
	auto *item_a = handler.last_item;

	handler.got_item = false;
	handler.last_item = nullptr;
	map.Get(StockKey{"b"}, nullptr, handler, cancel_ptr);
	auto *item_b = handler.last_item;

	ASSERT_EQ(cls.n_create, 2);

	/* release both so they become idle */

	item_a->Put(PutAction::REUSE);
	item_b->Put(PutAction::REUSE);
	instance.RunSome();
	ASSERT_EQ(num_destroy, 0);

	/* fade only key "a" */

	map.FadeKey(StockKey{"a"});
	ASSERT_EQ(num_destroy, 1);

	instance.RunSome();

	/* get from key "a" - should create a fresh item */

	handler.got_item = false;
	handler.last_item = nullptr;
	map.Get(StockKey{"a"}, nullptr, handler, cancel_ptr);
	ASSERT_TRUE(handler.got_item);
	ASSERT_EQ(cls.n_create, 3);
	auto *new_a = handler.last_item;

	/* get from key "b" - should reuse the existing idle item */

	handler.got_item = false;
	handler.last_item = nullptr;
	map.Get(StockKey{"b"}, nullptr, handler, cancel_ptr);
	ASSERT_TRUE(handler.got_item);
	ASSERT_EQ(handler.last_item, item_b);
	ASSERT_EQ(cls.n_create, 3);
	ASSERT_EQ(num_borrow, 1);

	/* clean up */

	new_a->Put(PutAction::DESTROY);
	item_b->Put(PutAction::DESTROY);
}

/**
 * Test FadeKey() on busy items: the targeted key's items are
 * faded but not destroyed until released; items under other keys
 * remain unaffected.
 */
TEST(StockMap, FadeKeyBusy)
{
	Instance instance;
	MyStockClass cls;
	StockMap map{
		instance.event_loop, cls,
		{
			.limit = 3,
			.max_idle = 8,
		},
	};

	MyStockGetHandler handler;
	CancellablePointer cancel_ptr;

	num_borrow = num_release = num_destroy = 0;

	/* create items under two keys */

	map.Get(StockKey{"a"}, nullptr, handler, cancel_ptr);
	auto *item_a = handler.last_item;

	handler.got_item = false;
	handler.last_item = nullptr;
	map.Get(StockKey{"b"}, nullptr, handler, cancel_ptr);
	auto *item_b = handler.last_item;

	ASSERT_EQ(cls.n_create, 2);

	/* fade key "a" while items are busy */

	map.FadeKey(StockKey{"a"});
	instance.RunSome();

	/* nothing destroyed yet because item is busy */
	ASSERT_EQ(num_destroy, 0);

	/* return item_a with REUSE; since it's faded, it will be
	   destroyed instead of going idle */

	item_a->Put(PutAction::REUSE);
	instance.RunSome();
	ASSERT_EQ(num_release, 0);
	ASSERT_EQ(num_destroy, 1);

	/* item_b should still work normally */

	item_b->Put(PutAction::REUSE);
	instance.RunSome();
	ASSERT_EQ(num_release, 1);
	ASSERT_EQ(num_destroy, 1);

	/* get from key "b" - should reuse the idle item */

	handler.got_item = false;
	handler.last_item = nullptr;
	map.Get(StockKey{"b"}, nullptr, handler, cancel_ptr);
	ASSERT_TRUE(handler.got_item);
	ASSERT_EQ(handler.last_item, item_b);
	ASSERT_EQ(cls.n_create, 2);
	ASSERT_EQ(num_borrow, 1);

	handler.last_item->Put(PutAction::DESTROY);
}

/**
 * Test AddStats(): verify that statistics are aggregated from all
 * stocks in the map.
 */
TEST(StockMap, Stats)
{
	Instance instance;
	MyStockClass cls;
	StockMap map{
		instance.event_loop, cls,
		{
			.limit = 3,
			.max_idle = 8,
		},
	};

	MyStockGetHandler handler;
	CancellablePointer cancel_ptr;

	num_borrow = num_release = num_destroy = 0;

	/* empty map */

	StockStats stats{};
	map.AddStats(stats);
	ASSERT_EQ(stats.busy, 0);
	ASSERT_EQ(stats.idle, 0);

	/* create items under two keys */

	map.Get(StockKey{"a"}, nullptr, handler, cancel_ptr);
	auto *item_a = handler.last_item;

	handler.got_item = false;
	handler.last_item = nullptr;
	map.Get(StockKey{"b"}, nullptr, handler, cancel_ptr);
	auto *item_b = handler.last_item;

	/* both busy */

	stats = {};
	map.AddStats(stats);
	ASSERT_EQ(stats.busy, 2);
	ASSERT_EQ(stats.idle, 0);

	/* release one - one busy, one idle */

	item_a->Put(PutAction::REUSE);
	instance.RunSome();

	stats = {};
	map.AddStats(stats);
	ASSERT_EQ(stats.busy, 1);
	ASSERT_EQ(stats.idle, 1);

	/* release the other - both idle */

	item_b->Put(PutAction::REUSE);
	instance.RunSome();

	stats = {};
	map.AddStats(stats);
	ASSERT_EQ(stats.busy, 0);
	ASSERT_EQ(stats.idle, 2);

	/* destroy one key's items */

	map.FadeKey(StockKey{"a"});

	stats = {};
	map.AddStats(stats);
	ASSERT_EQ(stats.busy, 0);
	ASSERT_EQ(stats.idle, 1);
}

/**
 * Test SetSticky(): a sticky stock is not removed from the map
 * when it becomes empty.
 */
TEST(StockMap, Sticky)
{
	Instance instance;
	MyStockClass cls;
	StockMap map{
		instance.event_loop, cls,
		{
			.limit = 3,
			.max_idle = 8,
		},
	};

	num_borrow = num_release = num_destroy = 0;

	/* create stock for key "a" and mark it sticky */

	Stock &stock_a = map.GetStock(StockKey{"a"}, nullptr);
	map.SetSticky(stock_a, true);

	/* create and destroy an item */

	auto *item = map.GetNow(StockKey{"a"}, nullptr);
	ASSERT_NE(item, nullptr);
	ASSERT_EQ(cls.n_create, 1);

	item->Put(PutAction::DESTROY);
	instance.RunSome();
	ASSERT_EQ(num_destroy, 1);

	/* stock should still exist because it's sticky */

	Stock &stock_a2 = map.GetStock(StockKey{"a"}, nullptr);
	ASSERT_EQ(&stock_a, &stock_a2);

	/* clear sticky; since the stock is empty, it gets erased */

	map.SetSticky(stock_a, false);

	/* creating a new item under the same key still works */

	auto *item2 = map.GetNow(StockKey{"a"}, nullptr);
	ASSERT_NE(item2, nullptr);
	ASSERT_EQ(cls.n_create, 2);

	item2->Put(PutAction::DESTROY);
}
