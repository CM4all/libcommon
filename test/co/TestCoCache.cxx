// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "co/Cache.hxx"
#include "co/Task.hxx"
#include "co/InvokeTask.hxx"
#include "event/co/Sleep.hxx"
#include "event/Loop.hxx"

#include <gtest/gtest.h>

#include <cassert>

static unsigned n_started, n_finished;

struct ImmediateFactory {
	Co::Task<int> operator()(int key) noexcept {
		++n_started;
		++n_finished;
		co_return key;
	}
};

struct IsCacheableFactory {
	Co::Task<int> operator()(int key) noexcept {
		++n_started;
		++n_finished;
		co_return key;
	}

	bool IsCacheable(int value) const noexcept {
		return value % 2 == 0;
	}
};

struct SleepFactory {
	EventLoop &event_loop;

	explicit SleepFactory(EventLoop &_event_loop) noexcept
		:event_loop(_event_loop) {}

	Co::Task<int> operator()(int key) noexcept {
		++n_started;
		co_await Co::Sleep(event_loop, std::chrono::milliseconds(1));
		++n_finished;
		co_return key;
	}
};

struct ThrowImmediateFactory {
	Co::Task<int> operator()(int) {
		++n_started;
		++n_finished;
		throw std::runtime_error("Error");;
	}
};

struct ThrowSleepFactory {
	EventLoop &event_loop;

	explicit ThrowSleepFactory(EventLoop &_event_loop) noexcept
		:event_loop(_event_loop) {}

	Co::Task<int> operator()(int) {
		++n_started;
		co_await Co::Sleep(event_loop, std::chrono::milliseconds(1));
		++n_finished;
		throw std::runtime_error("Error");;
	}
};

template<typename Factory>
using TestCache = Co::Cache<Factory, int, int, 2048, 2021>;

template<typename Factory>
struct Work {
	TestCache<Factory> &cache;

	Co::InvokeTask task;

	std::exception_ptr error;

	int value = -1;

	explicit Work(TestCache<Factory> &_cache) noexcept
		:cache(_cache) {}

	Co::InvokeTask Run(int key) {
		value = co_await cache.Get(key);
	}

	void Start(int key) {
		task = Run(key);
		task.Start(BIND_THIS_METHOD(OnCompletion));
	}

	void OnCompletion(std::exception_ptr _error) noexcept {
		assert(value >= 0 || _error);

		error = std::move(_error);
	}
};

TEST(CoCache, Cached)
{
	using Factory = ImmediateFactory;
	using Cache = TestCache<Factory>;

	Cache cache;

	n_started = n_finished = 0;

	Work w1(cache), w2(cache);
	w1.Start(42);
	w2.Start(42);

	ASSERT_EQ(w1.value, 42);
	ASSERT_EQ(w2.value, 42);
	ASSERT_EQ(n_started, 1u);
	ASSERT_EQ(n_finished, 1u);
	ASSERT_NE(cache.GetIfCached(42), nullptr);
	ASSERT_EQ(cache.GetIfCached(1), nullptr);
}

TEST(CoCache, Sleep)
{
	using Factory = SleepFactory;
	using Cache = TestCache<Factory>;

	EventLoop event_loop;
	Cache cache(event_loop);

	n_started = n_finished = 0;

	Work w1(cache), w2(cache), w3(cache), w4(cache);
	w1.Start(42);
	w2.Start(3);
	w3.Start(42);

	ASSERT_EQ(n_started, 2u);
	ASSERT_EQ(n_finished, 0u);
	ASSERT_EQ(cache.GetIfCached(42), nullptr);
	ASSERT_EQ(cache.GetIfCached(3), nullptr);

	event_loop.Run();

	ASSERT_EQ(w1.value, 42);
	ASSERT_EQ(w2.value, 3);
	ASSERT_EQ(w3.value, 42);
	ASSERT_EQ(n_started, 2u);
	ASSERT_EQ(n_finished, 2u);
	ASSERT_NE(cache.GetIfCached(42), nullptr);
	ASSERT_NE(cache.GetIfCached(3), nullptr);

	w4.Start(42);

	event_loop.Run();

	ASSERT_EQ(w4.value, 42);
	ASSERT_EQ(n_started, 2u);
	ASSERT_EQ(n_finished, 2u);

	// test Clear()

	{
		n_started = n_finished = 0;

		Work w5(cache), w6(cache);
		w5.Start(5);
		w6.Start(3);

		ASSERT_EQ(n_started, 1u);
		ASSERT_EQ(n_finished, 0u);

		/* this also marks the running request as "don't store" */
		cache.Clear();

		ASSERT_EQ(cache.GetIfCached(42), nullptr);
		ASSERT_EQ(cache.GetIfCached(5), nullptr);
		ASSERT_EQ(cache.GetIfCached(3), nullptr);

		event_loop.Run();

		ASSERT_EQ(w5.value, 5);
		ASSERT_EQ(w6.value, 3);
		ASSERT_EQ(n_started, 1u);
		ASSERT_EQ(n_finished, 1u);
		ASSERT_EQ(cache.GetIfCached(5), nullptr);
		ASSERT_EQ(cache.GetIfCached(3), nullptr);
	}

	{
		n_started = n_finished = 0;

		Work w5(cache), w6(cache);
		w5.Start(5);
		w6.Start(3);

		ASSERT_EQ(n_started, 2u);
		ASSERT_EQ(n_finished, 0u);

		event_loop.Run();

		ASSERT_EQ(w5.value, 5);
		ASSERT_EQ(w6.value, 3);
		ASSERT_EQ(n_started, 2u);
		ASSERT_EQ(n_finished, 2u);
	}


	// test Remove()

	ASSERT_NE(cache.GetIfCached(3), nullptr);
	ASSERT_NE(cache.GetIfCached(5), nullptr);
	cache.Remove(3);
	ASSERT_EQ(cache.GetIfCached(3), nullptr);
	ASSERT_NE(cache.GetIfCached(5), nullptr);
}

TEST(CoCache, CancelSingle)
{
	using Factory = SleepFactory;
	using Cache = TestCache<Factory>;

	EventLoop event_loop;
	Cache cache(event_loop);

	n_started = n_finished = 0;

	{
		Work w(cache);
		w.Start(42);
	}

	event_loop.Run();

	ASSERT_EQ(n_started, 1u);
	ASSERT_EQ(n_finished, 0u);
}

TEST(CoCache, CancelOne)
{
	using Factory = SleepFactory;
	using Cache = TestCache<Factory>;

	EventLoop event_loop;
	Cache cache(event_loop);

	n_started = n_finished = 0;

	Work w1(cache);
	w1.Start(42);

	{
		Work w2(cache);
		w2.Start(42);
	}

	ASSERT_EQ(n_started, 1u);
	ASSERT_EQ(n_finished, 0u);

	event_loop.Run();

	ASSERT_EQ(w1.value, 42);
	ASSERT_EQ(n_started, 1u);
	ASSERT_EQ(n_finished, 1u);
}

TEST(CoCache, CancelBothSingle)
{
	using Factory = SleepFactory;
	using Cache = TestCache<Factory>;

	EventLoop event_loop;
	Cache cache(event_loop);

	n_started = n_finished = 0;

	{
		Work w(cache);
		w.Start(42);
	}

	{
		Work w(cache);
		w.Start(42);
	}

	ASSERT_EQ(n_started, 2u);
	ASSERT_EQ(n_finished, 0u);

	event_loop.Run();

	ASSERT_EQ(n_started, 2u);
	ASSERT_EQ(n_finished, 0u);
}

TEST(CoCache, CancelAll)
{
	using Factory = SleepFactory;
	using Cache = TestCache<Factory>;

	EventLoop event_loop;
	Cache cache(event_loop);

	n_started = n_finished = 0;

	{
		Work w1(cache), w2(cache);
		w1.Start(42);
		w2.Start(42);
	}

	event_loop.Run();

	ASSERT_EQ(n_started, 1u);
	ASSERT_EQ(n_finished, 0u);
}

TEST(CoCache, ThrowImmediate)
{
	using Factory = ThrowImmediateFactory;
	using Cache = TestCache<Factory>;

	Cache cache;

	n_started = n_finished = 0;

	Work w1(cache), w2(cache);
	w1.Start(42);
	w2.Start(42);

	ASSERT_EQ(n_started, 2u);
	ASSERT_EQ(n_finished, 2u);
	ASSERT_TRUE(w1.error);
	ASSERT_TRUE(w2.error);

	Work w3(cache);
	w3.Start(42);

	ASSERT_EQ(n_started, 3u);
	ASSERT_EQ(n_finished, 3u);
	ASSERT_TRUE(w3.error);
}

TEST(CoCache, ThrowSleep)
{
	using Factory = ThrowSleepFactory;
	using Cache = TestCache<Factory>;

	EventLoop event_loop;
	Cache cache(event_loop);

	n_started = n_finished = 0;

	Work w1(cache), w2(cache), w3(cache), w4(cache);
	w1.Start(42);
	w2.Start(3);
	w3.Start(42);

	ASSERT_EQ(n_started, 2u);
	ASSERT_EQ(n_finished, 0u);

	event_loop.Run();

	ASSERT_EQ(n_started, 2u);
	ASSERT_EQ(n_finished, 2u);
	ASSERT_TRUE(w1.error);
	ASSERT_TRUE(w2.error);
	ASSERT_TRUE(w3.error);
}

TEST(CoCache, RemoveIf)
{
	using Factory = ImmediateFactory;
	using Cache = TestCache<Factory>;

	Cache cache;

	n_started = n_finished = 0;

	Work w1(cache), w2(cache), w3(cache), w4(cache);
	w1.Start(1);
	w2.Start(2);
	w3.Start(3);
	w4.Start(4);

	ASSERT_EQ(n_started, 4u);
	ASSERT_EQ(n_finished, 4u);
	ASSERT_EQ(w1.value, 1);
	ASSERT_EQ(w2.value, 2);
	ASSERT_EQ(w3.value, 3);
	ASSERT_EQ(w4.value, 4);
	ASSERT_EQ(*cache.GetIfCached(1), 1);
	ASSERT_EQ(*cache.GetIfCached(2), 2);
	ASSERT_EQ(*cache.GetIfCached(3), 3);
	ASSERT_EQ(*cache.GetIfCached(4), 4);

	cache.RemoveIf([](int, int value){
		return value % 2 == 0;
	});

	ASSERT_EQ(*cache.GetIfCached(1), 1);
	ASSERT_EQ(cache.GetIfCached(2), nullptr);
	ASSERT_EQ(*cache.GetIfCached(3), 3);
	ASSERT_EQ(cache.GetIfCached(4), nullptr);
}

TEST(CoCache, IsCacheable)
{
	EventLoop event_loop;

	using Factory = IsCacheableFactory;
	using Cache = TestCache<Factory>;

	Cache cache;

	n_started = n_finished = 0;

	Work w1(cache), w2(cache), w3(cache), w4(cache);
	w1.Start(1);
	w2.Start(2);
	w3.Start(3);
	w4.Start(4);

	ASSERT_EQ(n_started, 4u);
	ASSERT_EQ(n_finished, 4u);
	ASSERT_EQ(w1.value, 1);
	ASSERT_EQ(w2.value, 2);
	ASSERT_EQ(w3.value, 3);
	ASSERT_EQ(w4.value, 4);
	ASSERT_EQ(cache.GetIfCached(1), nullptr);
	ASSERT_EQ(*cache.GetIfCached(2), 2);
	ASSERT_EQ(cache.GetIfCached(3), nullptr);
	ASSERT_EQ(*cache.GetIfCached(4), 4);
}
