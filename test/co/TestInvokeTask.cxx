// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "PauseTask.hxx"
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

static Co::EagerTask<void>
EagerIncTask(int &i)
{
	++i;
	co_return;
}

static Co::InvokeTask
IncInvokeTask(int &i) noexcept
{
	++i;
	co_return;
}

static Co::InvokeTask
MakeInvokeTask(int &i, auto &task) noexcept
{
	++i;
	co_await task;
	++i;
}

static Co::Task<void>
ThrowTask(int &i)
{
	if (false)
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

static Co::EagerTask<void>
EagerWaiter(int &i, auto &task)
{
	++i;
	co_await task;
	++i;
}

TEST(InvokeTask, Basic)
{
	int i = 0;

	auto invoke = IncInvokeTask(i);
	ASSERT_TRUE(invoke);
	ASSERT_EQ(i, 0);

	Completion c;
	c.Start(invoke);

	ASSERT_TRUE(c.done);
	ASSERT_FALSE(c.error);
	ASSERT_EQ(i, 1);
}

TEST(InvokeTask, Task)
{
	int task_i = 0, invoke_i = 0;

	auto task = IncTask(task_i);

	auto invoke = MakeInvokeTask(invoke_i, task);
	ASSERT_TRUE(invoke);
	ASSERT_EQ(invoke_i, 0);
	ASSERT_EQ(task_i, 0);

	Completion c;
	c.Start(invoke);
	ASSERT_TRUE(c.done);
	ASSERT_FALSE(c.error);

	ASSERT_EQ(invoke_i, 2);
	ASSERT_EQ(task_i, 1);
}

TEST(InvokeTask, EagerTask)
{
	int task_i = 0, invoke_i = 0;

	auto task = EagerIncTask(task_i);

	auto invoke = MakeInvokeTask(invoke_i, task);
	ASSERT_TRUE(invoke);
	ASSERT_EQ(invoke_i, 0);
	ASSERT_EQ(task_i, 1);

	Completion c;
	c.Start(invoke);
	ASSERT_TRUE(c.done);
	ASSERT_FALSE(c.error);

	ASSERT_EQ(invoke_i, 2);
	ASSERT_EQ(task_i, 1);
}

TEST(InvokeTask, Throw)
{
	int task_i = 0, invoke_i = 0;

	auto task = ThrowTask(task_i);

	auto invoke = MakeInvokeTask(invoke_i, task);
	ASSERT_TRUE(invoke);
	ASSERT_EQ(invoke_i, 0);
	ASSERT_EQ(task_i, 0);

	Completion c;
	c.Start(invoke);
	ASSERT_TRUE(c.done);
	ASSERT_TRUE(c.error);

	ASSERT_EQ(invoke_i, 1);
	ASSERT_EQ(task_i, 1);
}

TEST(InvokeTask, Pause)
{
	int task_i = 0, invoke_i = 0;

	Co::PauseTask pause;
	auto task = Waiter(task_i, pause);

	auto invoke = MakeInvokeTask(invoke_i, task);
	ASSERT_TRUE(invoke);
	ASSERT_EQ(invoke_i, 0);
	ASSERT_EQ(task_i, 0);

	Completion c;
	c.Start(invoke);
	ASSERT_FALSE(c.done);
	ASSERT_FALSE(c.error);

	ASSERT_EQ(invoke_i, 1);
	ASSERT_EQ(task_i, 1);

	pause.Resume();

	ASSERT_TRUE(c.done);
	ASSERT_FALSE(c.error);
	ASSERT_EQ(invoke_i, 2);
	ASSERT_EQ(task_i, 2);
}

TEST(InvokeTask, PauseEager)
{
	int task_i = 0, invoke_i = 0;

	Co::PauseTask pause;
	auto task = EagerWaiter(task_i, pause);

	auto invoke = MakeInvokeTask(invoke_i, task);
	ASSERT_TRUE(invoke);
	ASSERT_EQ(invoke_i, 0);
	ASSERT_EQ(task_i, 1);

	Completion c;
	c.Start(invoke);
	ASSERT_FALSE(c.done);
	ASSERT_FALSE(c.error);

	ASSERT_EQ(invoke_i, 1);
	ASSERT_EQ(task_i, 1);

	pause.Resume();

	ASSERT_TRUE(c.done);
	ASSERT_FALSE(c.error);
	ASSERT_EQ(invoke_i, 2);
	ASSERT_EQ(task_i, 2);
}
