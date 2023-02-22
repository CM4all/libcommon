// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "PauseTask.hxx"
#include "co/All.hxx"
#include "co/InvokeTask.hxx"
#include "co/Task.hxx"

#include <gtest/gtest.h>

#include <cassert>

struct Completion {
	std::exception_ptr error;
	bool done = false;

	void Callback(std::exception_ptr _error) noexcept {
		assert(!done);
		assert(!error);

		error = std::move(_error);
		done = true;
	}

	void Start(Co::InvokeTask &invoke) noexcept {
		assert(invoke);
		invoke.Start(BIND_THIS_METHOD(Callback));
		assert(invoke);
	}
};

static Co::Task<void>
IncTask(int &i)
{
	++i;
	co_return;
}

static Co::Task<void>
ThrowTask(int &i)
{
	if constexpr (false)
		/* force this to be a coroutine */
		co_return;

	++i;
	throw "error";
}

static Co::Task<void>
Waiter(int &i, auto &task)
{
	++i;
	co_await task;
	++i;
}

template<typename... Tasks>
static Co::InvokeTask
AwaitAll(int &i, Tasks&... tasks)
{
	++i;
	co_await Co::All{tasks...};
	++i;

	(co_await tasks, ...);
}

TEST(All, Basic)
{
	int i = 0, j = 0, k = 0;

	auto task1 = IncTask(i);
	auto task2 = IncTask(j);

	auto invoke = AwaitAll(k, task1, task2);
	ASSERT_TRUE(invoke);
	ASSERT_FALSE(invoke.done());
	ASSERT_EQ(i, 0);
	ASSERT_EQ(j, 0);
	ASSERT_EQ(k, 0);

	Completion c;
	c.Start(invoke);

	ASSERT_TRUE(invoke.done());
	ASSERT_TRUE(c.done);
	ASSERT_FALSE(c.error);
	ASSERT_EQ(i, 1);
	ASSERT_EQ(j, 1);
	ASSERT_EQ(k, 2);
}

TEST(All, Cancel)
{
	int i = 0, j = 0, k = 0;

	auto task1 = IncTask(i);
	auto task2 = IncTask(j);

	auto invoke = AwaitAll(k, task1, task2);
	ASSERT_TRUE(invoke);
	ASSERT_FALSE(invoke.done());
	ASSERT_EQ(i, 0);
	ASSERT_EQ(j, 0);
	ASSERT_EQ(k, 0);
}

TEST(All, FirstBlocks)
{
	int i = 0, j = 0, k = 0;

	Co::PauseTask pause;
	auto task1 = Waiter(i, pause);
	auto task2 = IncTask(j);

	auto invoke = AwaitAll(k, task1, task2);
	ASSERT_TRUE(invoke);
	ASSERT_FALSE(invoke.done());
	ASSERT_EQ(i, 0);
	ASSERT_EQ(j, 0);
	ASSERT_EQ(k, 0);

	Completion c;
	c.Start(invoke);

	ASSERT_FALSE(invoke.done());
	ASSERT_FALSE(c.done);
	ASSERT_EQ(i, 1);
	ASSERT_EQ(j, 1);
	ASSERT_EQ(k, 1);

	pause.Resume();

	ASSERT_TRUE(invoke.done());
	ASSERT_TRUE(c.done);
	ASSERT_FALSE(c.error);
	ASSERT_EQ(i, 2);
	ASSERT_EQ(j, 1);
	ASSERT_EQ(k, 2);
}

TEST(All, SecondBlocks)
{
	int i = 0, j = 0, k = 0;

	Co::PauseTask pause;
	auto task1 = IncTask(i);
	auto task2 = Waiter(j, pause);

	auto invoke = AwaitAll(k, task1, task2);
	ASSERT_TRUE(invoke);
	ASSERT_FALSE(invoke.done());
	ASSERT_EQ(i, 0);
	ASSERT_EQ(j, 0);
	ASSERT_EQ(k, 0);

	Completion c;
	c.Start(invoke);

	ASSERT_FALSE(invoke.done());
	ASSERT_FALSE(c.done);
	ASSERT_EQ(i, 1);
	ASSERT_EQ(j, 1);
	ASSERT_EQ(k, 1);

	pause.Resume();

	ASSERT_TRUE(invoke.done());
	ASSERT_TRUE(c.done);
	ASSERT_FALSE(c.error);
	ASSERT_EQ(i, 1);
	ASSERT_EQ(j, 2);
	ASSERT_EQ(k, 2);
}

TEST(All, CancelBlocking)
{
	int i = 0, j = 0, k = 0;

	Co::PauseTask pause1;
	Co::PauseTask pause2;
	auto task1 = Waiter(i, pause1);
	auto task2 = Waiter(j, pause2);

	auto invoke = AwaitAll(k, task1, task2);
	ASSERT_TRUE(invoke);
	ASSERT_FALSE(invoke.done());
	ASSERT_EQ(i, 0);
	ASSERT_EQ(j, 0);
	ASSERT_EQ(k, 0);

	Completion c;
	c.Start(invoke);

	ASSERT_FALSE(invoke.done());
	ASSERT_FALSE(c.done);
	ASSERT_EQ(i, 1);
	ASSERT_EQ(j, 1);
	ASSERT_EQ(k, 1);
}

TEST(All, FirstThrows)
{
	int i = 0, j = 0, k = 0;

	auto task1 = ThrowTask(i);
	auto task2 = IncTask(j);

	auto invoke = AwaitAll(k, task1, task2);
	ASSERT_TRUE(invoke);
	ASSERT_FALSE(invoke.done());
	ASSERT_EQ(i, 0);
	ASSERT_EQ(j, 0);
	ASSERT_EQ(k, 0);

	Completion c;
	c.Start(invoke);

	ASSERT_TRUE(invoke.done());
	ASSERT_TRUE(c.done);
	ASSERT_TRUE(c.error);
	ASSERT_EQ(i, 1);
	ASSERT_EQ(j, 1);
	ASSERT_EQ(k, 2);
}
