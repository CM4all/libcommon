// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "PauseTask.hxx"
#include "co/MultiAwaitable.hxx"
#include "co/Task.hxx"

#include <gtest/gtest.h>

static Co::Task<void>
NoTask()
{
	co_return;
}

static Co::Task<void>
MyTask(auto &&task)
{
	co_await task;
}

template<typename T>
static Co::EagerTask<void>
Waiter(T &&task, bool &complete_r)
{
	assert(!complete_r);
	co_await task;

	assert(!complete_r);
	complete_r = true;
}

TEST(MultiAwaitable, CompleteEarly)
{
	Co::MultiAwaitable m{NoTask()};

	std::array complete{false, false, false};
	std::array waiters{
		Waiter(m, complete[0]),
		Waiter(m, complete[1]),
	};

	ASSERT_TRUE(complete[0]);
	ASSERT_TRUE(complete[1]);
}

TEST(MultiAwaitable, CompleteLate)
{
	Co::PauseTask pause;
	Co::MultiAwaitable m{MyTask(pause)};

	ASSERT_TRUE(pause.IsAwaited());

	std::array complete{false, false, false};
	std::array waiters{
		Waiter(m, complete[0]),
		Waiter(m, complete[1]),
		Waiter(m, complete[2]),
	};

	ASSERT_FALSE(complete[0]);
	ASSERT_FALSE(complete[1]);
	ASSERT_FALSE(complete[2]);

	ASSERT_TRUE(pause.IsAwaited());
	pause.Resume();

	ASSERT_TRUE(complete[0]);
	ASSERT_TRUE(complete[1]);
	ASSERT_TRUE(complete[2]);

	/* add another waiter which doesn't suspend because the
	   MultiAwaitable is ready already */
	bool complete3 = false;
	auto waiter3 = Waiter(m, complete3);
	ASSERT_TRUE(complete3);
}

TEST(MultiAwaitable, CancelOne)
{
	Co::PauseTask pause;
	Co::MultiAwaitable m{MyTask(pause)};

	ASSERT_TRUE(pause.IsAwaited());

	std::array complete{false, false, false};

	auto w0 = Waiter(m, complete[0]);
	std::optional w1 = Waiter(m, complete[1]);
	auto w2 = Waiter(m, complete[2]);

	ASSERT_FALSE(complete[0]);
	ASSERT_FALSE(complete[1]);
	ASSERT_FALSE(complete[2]);

	w1.reset();

	ASSERT_TRUE(pause.IsAwaited());
	pause.Resume();

	ASSERT_TRUE(complete[0]);
	ASSERT_FALSE(complete[1]);
	ASSERT_TRUE(complete[2]);
}

TEST(MultiAwaitable, CancelAll)
{
	Co::PauseTask pause;
	Co::MultiAwaitable m{MyTask(pause)};

	ASSERT_TRUE(pause.IsAwaited());

	std::array complete{false, false, false};

	std::optional w0 = Waiter(m, complete[0]);
	std::optional w1 = Waiter(m, complete[1]);
	std::optional w2 = Waiter(m, complete[2]);

	ASSERT_FALSE(complete[0]);
	ASSERT_FALSE(complete[1]);
	ASSERT_FALSE(complete[2]);

	ASSERT_TRUE(pause.IsAwaited());
	w0.reset();
	ASSERT_TRUE(pause.IsAwaited());
	w1.reset();
	ASSERT_TRUE(pause.IsAwaited());
	w2.reset();
	ASSERT_FALSE(pause.IsAwaited());

	ASSERT_FALSE(complete[0]);
	ASSERT_FALSE(complete[1]);
	ASSERT_FALSE(complete[2]);
}

TEST(MultiAwaitable, Reuse)
{
	Co::MultiAwaitable m;
	ASSERT_FALSE(m.IsActive());

	/* complete one */
	{
		Co::PauseTask pause;
		m.Start(MyTask(pause));
		ASSERT_TRUE(m.IsActive());

		bool complete = false;
		auto w = Waiter(m, complete);
		ASSERT_FALSE(complete);
		ASSERT_TRUE(pause.IsAwaited());

		pause.Resume();
		ASSERT_TRUE(complete);
		ASSERT_FALSE(m.IsActive());
	}

	/* cancel */
	{
		Co::PauseTask pause;
		m.Start(MyTask(pause));
		ASSERT_TRUE(m.IsActive());

		bool complete = false;
		std::optional w = Waiter(m, complete);
		ASSERT_TRUE(m.IsActive());
		ASSERT_TRUE(pause.IsAwaited());

		w.reset();
		ASSERT_FALSE(complete);
		ASSERT_FALSE(m.IsActive());
	}

	/* complete another one */
	{
		Co::PauseTask pause;
		m.Start(MyTask(pause));
		ASSERT_TRUE(m.IsActive());

		bool complete = false;
		auto w = Waiter(m, complete);
		ASSERT_FALSE(complete);
		ASSERT_TRUE(pause.IsAwaited());

		pause.Resume();
		ASSERT_TRUE(complete);
	}
}
