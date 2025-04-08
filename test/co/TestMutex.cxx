// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "PauseTask.hxx"
#include "co/Mutex.hxx"
#include "co/Task.hxx"

#include <gtest/gtest.h>

static Co::EagerTask<void>
LockTask(Co::Mutex &mutex, bool &complete_r)
{
	const auto lock = co_await mutex;
	complete_r = true;
}

static Co::EagerTask<void>
LockTask(Co::Mutex &mutex, Co::PauseTask &pause, bool &complete_r)
{
	const auto lock = co_await mutex;
	complete_r = true;
	co_await pause;
}

TEST(Mutex, One)
{
	Co::Mutex m;

	std::array complete{false};
	std::array waiters{
		LockTask(m, complete[0]),
	};

	EXPECT_TRUE(complete[0]);
}

TEST(Mutex, ThreeUncontended)
{
	Co::Mutex m;

	std::array complete{false, false, false};
	std::array waiters{
		LockTask(m, complete[0]),
		LockTask(m, complete[1]),
		LockTask(m, complete[2]),
	};

	EXPECT_TRUE(complete[0]);
	EXPECT_TRUE(complete[1]);
	EXPECT_TRUE(complete[2]);
}

TEST(Mutex, ThreeContended)
{
	Co::Mutex m;

	std::array complete{false, false, false};
	std::array<Co::PauseTask, 2> pause;
	std::array waiters{
		LockTask(m, pause[0], complete[0]),
		LockTask(m, pause[1], complete[1]),
		LockTask(m, complete[2]),
	};

	EXPECT_TRUE(complete[0]);
	EXPECT_FALSE(complete[1]);
	EXPECT_FALSE(complete[2]);

	EXPECT_TRUE(pause[0].IsAwaited());
	EXPECT_FALSE(pause[1].IsAwaited());

	pause[0].Resume();

	EXPECT_TRUE(complete[0]);
	EXPECT_TRUE(complete[1]);
	EXPECT_FALSE(complete[2]);

	EXPECT_FALSE(pause[0].IsAwaited());
	EXPECT_TRUE(pause[1].IsAwaited());

	pause[1].Resume();

	EXPECT_TRUE(complete[0]);
	EXPECT_TRUE(complete[1]);
	EXPECT_TRUE(complete[2]);

	EXPECT_FALSE(pause[0].IsAwaited());
	EXPECT_FALSE(pause[1].IsAwaited());
}

TEST(Mutex, CancelHolder)
{
	Co::Mutex m;

	std::array complete{false, false, false};
	std::array<Co::PauseTask, 2> pause;
	std::array waiters{
		LockTask(m, pause[0], complete[0]),
		LockTask(m, pause[1], complete[1]),
		LockTask(m, complete[2]),
	};

	EXPECT_TRUE(complete[0]);
	EXPECT_FALSE(complete[1]);
	EXPECT_FALSE(complete[2]);

	EXPECT_TRUE(pause[0].IsAwaited());
	EXPECT_FALSE(pause[1].IsAwaited());

	// cancel [0] which releases the lock
	waiters[0] = {};

	EXPECT_TRUE(complete[0]);
	EXPECT_TRUE(complete[1]);
	EXPECT_FALSE(complete[2]);

	EXPECT_FALSE(pause[0].IsAwaited());
	EXPECT_TRUE(pause[1].IsAwaited());

	pause[1].Resume();

	EXPECT_TRUE(complete[0]);
	EXPECT_TRUE(complete[1]);
	EXPECT_TRUE(complete[2]);

	EXPECT_FALSE(pause[0].IsAwaited());
	EXPECT_FALSE(pause[1].IsAwaited());
}

TEST(Mutex, CancelWaiter)
{
	Co::Mutex m;

	std::array complete{false, false, false};
	std::array<Co::PauseTask, 2> pause;
	std::array waiters{
		LockTask(m, pause[0], complete[0]),
		LockTask(m, pause[1], complete[1]),
		LockTask(m, complete[2]),
	};

	EXPECT_TRUE(complete[0]);
	EXPECT_FALSE(complete[1]);
	EXPECT_FALSE(complete[2]);

	EXPECT_TRUE(pause[0].IsAwaited());
	EXPECT_FALSE(pause[1].IsAwaited());

	// cancel [1] = no change, [9] is still holding lock
	waiters[1] = {};

	EXPECT_TRUE(complete[0]);
	EXPECT_FALSE(complete[1]);
	EXPECT_FALSE(complete[2]);

	EXPECT_TRUE(pause[0].IsAwaited());
	EXPECT_FALSE(pause[1].IsAwaited());

	// this resumes [2] because [1] was canceled
	pause[0].Resume();

	EXPECT_TRUE(complete[0]);
	EXPECT_FALSE(complete[1]);
	EXPECT_TRUE(complete[2]);

	EXPECT_FALSE(pause[0].IsAwaited());
	EXPECT_FALSE(pause[1].IsAwaited());
}
