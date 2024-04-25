// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "PauseTask.hxx"
#include "co/MultiAwaitable.hxx"
#include "co/Task.hxx"

#include <gtest/gtest.h>

#include <cassert>

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

	std::array complete{false, false};
	std::array waiters{
		Waiter(m, complete[0]),
		Waiter(m, complete[1]),
	};

	ASSERT_TRUE(complete[0]);
	ASSERT_TRUE(complete[1]);
}

/**
 * Same as CompleteEarly(), but launch the task with Start(), which
 * uses the MultiAwaitable::MultiTask move operator.
 */
TEST(MultiAwaitable, CompleteEarlyStart)
{
	Co::MultiAwaitable m;
	m.Start(NoTask());

	std::array complete{false, false};
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

/**
 * Delete the MultiAwaitable in the continuation.
 */
TEST(MultiAwaitable, ResumeDelete)
{
	Co::PauseTask pause;
	ASSERT_FALSE(pause.IsAwaited());

	auto m = std::make_unique<Co::MultiAwaitable>(MyTask(pause));
	ASSERT_TRUE(m->IsActive());
	ASSERT_TRUE(pause.IsAwaited());

	auto task = [&]() -> Co::EagerTask<void> {
		co_await *m;
		m.reset();
		co_return;
	};

	ASSERT_TRUE(m);
	ASSERT_TRUE(m->IsActive());
	ASSERT_TRUE(pause.IsAwaited());

	auto waiter = task();

	ASSERT_TRUE(m);
	ASSERT_TRUE(pause.IsAwaited());
	pause.Resume();

	ASSERT_FALSE(m);
}

/**
 * Resumed first waiter adds another waiter from within a
 * continuation, and this waiter must not suspend at all because the
 * MultiAwaitable is ready.
 */
TEST(MultiAwaitable, ResumeAdd)
{
	Co::PauseTask pause;
	ASSERT_FALSE(pause.IsAwaited());

	Co::PauseTask pause2;
	ASSERT_FALSE(pause2.IsAwaited());

	Co::MultiAwaitable m{MyTask(pause)};
	ASSERT_TRUE(m.IsActive());
	ASSERT_TRUE(pause.IsAwaited());
	ASSERT_FALSE(pause2.IsAwaited());

	bool complete1 = false, complete2 = false;

	auto task1 = [&]() -> Co::EagerTask<void> {
		co_await m;
		pause2.Resume();
		complete1 = true;
		co_return;
	};

	auto task2 = [&]() -> Co::EagerTask<void> {
		co_await pause2;
		co_await m;
		complete2 = true;
		co_return;
	};

	ASSERT_TRUE(m.IsActive());
	ASSERT_TRUE(pause.IsAwaited());
	ASSERT_FALSE(pause2.IsAwaited());
	ASSERT_FALSE(complete1);
	ASSERT_FALSE(complete2);

	auto waiter1 = task1();

	ASSERT_TRUE(m.IsActive());
	ASSERT_TRUE(pause.IsAwaited());
	ASSERT_FALSE(pause2.IsAwaited());
	ASSERT_FALSE(complete1);
	ASSERT_FALSE(complete2);

	auto waiter2 = task2();

	ASSERT_TRUE(m.IsActive());
	ASSERT_TRUE(pause.IsAwaited());
	ASSERT_TRUE(pause2.IsAwaited());
	ASSERT_FALSE(complete1);
	ASSERT_FALSE(complete2);

	pause.Resume();

	ASSERT_TRUE(complete1);
	ASSERT_TRUE(complete2);
}

/**
 * Cancel the second waiter from within a continuation.  This tests
 * whether the MultiAwaitable's resume loop handles this case
 * properly.
 */
TEST(MultiAwaitable, ResumeCancel)
{
	Co::PauseTask pause;
	ASSERT_FALSE(pause.IsAwaited());

	Co::MultiAwaitable m{MyTask(pause)};
	ASSERT_TRUE(m.IsActive());
	ASSERT_TRUE(pause.IsAwaited());

	bool complete1 = false, complete2 = false;

	Co::EagerTask<void> waiter2;

	auto task1 = [&]() -> Co::EagerTask<void> {
		assert(!waiter2.IsDefined());

		co_await m;

		assert(waiter2.IsDefined());
		waiter2 = {};
		assert(!waiter2.IsDefined());

		complete1 = true;
		co_return;
	};

	auto task2 = [&]() -> Co::EagerTask<void> {
		co_await m;
		complete2 = true;
		co_return;
	};

	ASSERT_TRUE(m.IsActive());
	ASSERT_TRUE(pause.IsAwaited());
	ASSERT_FALSE(complete1);
	ASSERT_FALSE(complete2);

	auto waiter1 = task1();

	ASSERT_TRUE(m.IsActive());
	ASSERT_TRUE(pause.IsAwaited());
	ASSERT_FALSE(complete1);
	ASSERT_FALSE(complete2);

	waiter2 = task2();

	ASSERT_TRUE(m.IsActive());
	ASSERT_TRUE(pause.IsAwaited());
	ASSERT_FALSE(complete1);
	ASSERT_FALSE(complete2);

	pause.Resume();

	ASSERT_TRUE(complete1);
	ASSERT_FALSE(complete2);
}

/**
 * Like ResumeCancel, but delete the MultiAwaitable.
 */
TEST(MultiAwaitable, ResumeCancelDelete)
{
	Co::PauseTask pause;
	ASSERT_FALSE(pause.IsAwaited());

	auto m = std::make_unique<Co::MultiAwaitable>(MyTask(pause));
	ASSERT_TRUE(m->IsActive());
	ASSERT_TRUE(pause.IsAwaited());

	bool complete1 = false, complete2 = false;

	Co::EagerTask<void> waiter2;

	auto task1 = [&]() -> Co::EagerTask<void> {
		assert(!waiter2.IsDefined());

		co_await *m;

		assert(waiter2.IsDefined());
		waiter2 = {};
		assert(!waiter2.IsDefined());

		complete1 = true;

		m.reset();
		co_return;
	};

	auto task2 = [&]() -> Co::EagerTask<void> {
		co_await *m;
		complete2 = true;
		co_return;
	};

	ASSERT_TRUE(m->IsActive());
	ASSERT_TRUE(pause.IsAwaited());
	ASSERT_FALSE(complete1);
	ASSERT_FALSE(complete2);

	auto waiter1 = task1();

	ASSERT_TRUE(m->IsActive());
	ASSERT_TRUE(pause.IsAwaited());
	ASSERT_FALSE(complete1);
	ASSERT_FALSE(complete2);

	waiter2 = task2();

	ASSERT_TRUE(m->IsActive());
	ASSERT_TRUE(pause.IsAwaited());
	ASSERT_FALSE(complete1);
	ASSERT_FALSE(complete2);

	pause.Resume();

	ASSERT_TRUE(complete1);
	ASSERT_FALSE(complete2);
}
