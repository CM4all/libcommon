// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "stock/Stock.hxx"
#include "stock/Class.hxx"
#include "stock/GetHandler.hxx"
#include "stock/Item.hxx"
#include "event/Loop.hxx"
#include "util/Cancellable.hxx"
#include "util/PrintException.hxx"

#include <gtest/gtest.h>

#include <stdexcept>

#include <assert.h>

namespace {

static unsigned num_create, num_fail, num_borrow, num_release, num_destroy;
static bool next_fail;
static bool got_item;
static StockItem *last_item;

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

/*
 * stock class
 *
 */

class MyStockClass final : public StockClass {
public:
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
		++num_fail;
		delete item;
		throw std::runtime_error("next_fail");
	} else {
		++num_create;
		item->InvokeCreateSuccess(handler);
	}
}

class MyStockGetHandler final : public StockGetHandler {
public:
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

TEST(Stock, Basic)
{
	CancellablePointer cancel_ptr;
	StockItem *item, *second, *third;

	Instance instance;

	MyStockClass cls;
	Stock stock(instance.event_loop, cls, "test", 3, 8,
		    Event::Duration::zero());

	MyStockGetHandler handler;

	/* create first item */

	stock.Get(nullptr, handler, cancel_ptr);
	ASSERT_TRUE(got_item);
	ASSERT_NE(last_item, nullptr);
	ASSERT_EQ(num_create, 1);
	ASSERT_EQ(num_fail, 0);
	ASSERT_EQ(num_borrow, 0);
	ASSERT_EQ(num_release, 0);
	ASSERT_EQ(num_destroy, 0);
	item = last_item;

	/* release first item */

	stock.Put(*item, PutAction::REUSE);
	instance.RunSome();
	ASSERT_EQ(num_create, 1);
	ASSERT_EQ(num_fail, 0);
	ASSERT_EQ(num_borrow, 0);
	ASSERT_EQ(num_release, 1);
	ASSERT_EQ(num_destroy, 0);

	/* reuse first item */

	got_item = false;
	last_item = nullptr;
	stock.Get(nullptr, handler, cancel_ptr);
	ASSERT_TRUE(got_item);
	ASSERT_EQ(last_item, item);
	ASSERT_EQ(num_create, 1);
	ASSERT_EQ(num_fail, 0);
	ASSERT_EQ(num_borrow, 1);
	ASSERT_EQ(num_release, 1);
	ASSERT_EQ(num_destroy, 0);

	/* create second item */

	got_item = false;
	last_item = nullptr;
	stock.Get(nullptr, handler, cancel_ptr);
	ASSERT_TRUE(got_item);
	ASSERT_NE(last_item, nullptr);
	ASSERT_NE(last_item, item);
	ASSERT_EQ(num_create, 2);
	ASSERT_EQ(num_fail, 0);
	ASSERT_EQ(num_borrow, 1);
	ASSERT_EQ(num_release, 1);
	ASSERT_EQ(num_destroy, 0);
	second = last_item;

	/* fail to create third item */

	next_fail = true;
	got_item = false;
	last_item = nullptr;
	stock.Get(nullptr, handler, cancel_ptr);
	ASSERT_TRUE(got_item);
	ASSERT_EQ(last_item, nullptr);
	ASSERT_EQ(num_create, 2);
	ASSERT_EQ(num_fail, 1);
	ASSERT_EQ(num_borrow, 1);
	ASSERT_EQ(num_release, 1);
	ASSERT_EQ(num_destroy, 1);

	/* create third item */

	next_fail = false;
	got_item = false;
	last_item = nullptr;
	stock.Get(nullptr, handler, cancel_ptr);
	ASSERT_TRUE(got_item);
	ASSERT_NE(last_item, nullptr);
	ASSERT_EQ(num_create, 3);
	ASSERT_EQ(num_fail, 1);
	ASSERT_EQ(num_borrow, 1);
	ASSERT_EQ(num_release, 1);
	ASSERT_EQ(num_destroy, 1);
	third = last_item;

	/* fourth item waiting */

	got_item = false;
	last_item = nullptr;
	stock.Get(nullptr, handler, cancel_ptr);
	ASSERT_FALSE(got_item);
	ASSERT_EQ(num_create, 3);
	ASSERT_EQ(num_fail, 1);
	ASSERT_EQ(num_borrow, 1);
	ASSERT_EQ(num_release, 1);
	ASSERT_EQ(num_destroy, 1);

	/* fifth item waiting */

	stock.Get(nullptr, handler, cancel_ptr);
	ASSERT_FALSE(got_item);
	ASSERT_EQ(num_create, 3);
	ASSERT_EQ(num_fail, 1);
	ASSERT_EQ(num_borrow, 1);
	ASSERT_EQ(num_release, 1);
	ASSERT_EQ(num_destroy, 1);

	/* return third item */

	stock.Put(*third, PutAction::REUSE);
	instance.RunSome();
	ASSERT_EQ(num_create, 3);
	ASSERT_EQ(num_fail, 1);
	ASSERT_EQ(num_borrow, 2);
	ASSERT_EQ(num_release, 2);
	ASSERT_EQ(num_destroy, 1);
	ASSERT_TRUE(got_item);
	ASSERT_EQ(last_item, third);

	/* destroy second item */

	got_item = false;
	last_item = nullptr;
	stock.Put(*second, PutAction::DESTROY);
	instance.RunSome();
	ASSERT_EQ(num_create, 4);
	ASSERT_EQ(num_fail, 1);
	ASSERT_EQ(num_borrow, 2);
	ASSERT_EQ(num_release, 2);
	ASSERT_EQ(num_destroy, 2);
	ASSERT_TRUE(got_item);
	ASSERT_NE(last_item, nullptr);
	second = last_item;

	/* destroy first item */

	stock.Put(*item, PutAction::DESTROY);
	ASSERT_EQ(num_create, 4);
	ASSERT_EQ(num_fail, 1);
	ASSERT_EQ(num_borrow, 2);
	ASSERT_EQ(num_release, 2);
	ASSERT_EQ(num_destroy, 3);

	/* destroy second item */

	stock.Put(*second, PutAction::DESTROY);
	ASSERT_EQ(num_create, 4);
	ASSERT_EQ(num_fail, 1);
	ASSERT_EQ(num_borrow, 2);
	ASSERT_EQ(num_release, 2);
	ASSERT_EQ(num_destroy, 4);

	/* destroy third item */

	stock.Put(*third, PutAction::DESTROY);
	ASSERT_EQ(num_create, 4);
	ASSERT_EQ(num_fail, 1);
	ASSERT_EQ(num_borrow, 2);
	ASSERT_EQ(num_release, 2);
	ASSERT_EQ(num_destroy, 5);
}
