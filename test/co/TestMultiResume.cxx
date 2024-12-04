// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "co/MultiResume.hxx"
#include "co/Task.hxx"

#include <gtest/gtest.h>

static Co::EagerTask<void>
Waiter(auto &&task, bool &complete_r)
{
	assert(!complete_r);
	co_await task;

	assert(!complete_r);
	complete_r = true;
}

TEST(MultiResume, Nothing)
{
	[[maybe_unused]]
	Co::MultiResume m;
}

TEST(MultiResume, ResumeNone)
{
	Co::MultiResume m;
	m.ResumeAll();
}

TEST(MultiResume, ResumeOne)
{
	Co::MultiResume m;

	bool complete = false;
	auto w = Waiter(m, complete);

	EXPECT_FALSE(complete);

	m.ResumeAll();
	EXPECT_TRUE(complete);
}

TEST(MultiResume, ResumeTwice)
{
	Co::MultiResume m;
	m.ResumeAll();

	bool complete = false;
	auto w = Waiter(m, complete);

	EXPECT_FALSE(complete);

	m.ResumeAll();
	EXPECT_TRUE(complete);
}

TEST(MultiResume, ResumeTwo)
{
	Co::MultiResume m;
	m.ResumeAll();

	std::array complete{false, false};
	std::array waiters{
		Waiter(m, complete[0]),
		Waiter(m, complete[1]),
	};

	EXPECT_FALSE(complete[0]);
	EXPECT_FALSE(complete[1]);

	m.ResumeAll();
	EXPECT_TRUE(complete[0]);
	EXPECT_TRUE(complete[1]);
}

TEST(MultiResume, Cancel)
{
	Co::MultiResume m;
	m.ResumeAll();

	bool complete = false;
	auto w = Waiter(m, complete);

	EXPECT_FALSE(complete);
	w = {};
	EXPECT_FALSE(complete);

	m.ResumeAll();
	EXPECT_FALSE(complete);

	w = Waiter(m, complete);
	EXPECT_FALSE(complete);

	m.ResumeAll();
	EXPECT_TRUE(complete);
}

TEST(MultiResume, CancelOne)
{
	Co::MultiResume m;
	m.ResumeAll();

	std::array complete{false, false};
	std::array waiters{
		Waiter(m, complete[0]),
		Waiter(m, complete[1]),
	};

	EXPECT_FALSE(complete[0]);
	EXPECT_FALSE(complete[1]);

	waiters[1] = {};

	m.ResumeAll();
	EXPECT_TRUE(complete[0]);
	EXPECT_FALSE(complete[1]);
}

static Co::EagerTask<void>
CancelOtherTaskWaiter(auto &&task, bool &complete_r, auto &cancel_task)
{
	assert(!complete_r);
	co_await task;

	cancel_task = {};

	assert(!complete_r);
	complete_r = true;
}

/**
 * One resumed task cancels another (suspended) task.
 */
TEST(MultiResume, CancelInTask)
{
	Co::MultiResume m;
	m.ResumeAll();

	std::array complete{false, false};
	std::array<Co::EagerTask<void>, 2> waiters;
	waiters[0] = CancelOtherTaskWaiter(m, complete[0], waiters[1]);
	waiters[1] = Waiter(m, complete[1]);

	EXPECT_FALSE(complete[0]);
	EXPECT_FALSE(complete[1]);

	m.ResumeAll();
	EXPECT_TRUE(complete[0]);
	EXPECT_FALSE(complete[1]);
}

static Co::EagerTask<void>
AwaitOtherTaskWaiter(auto &&task, bool &complete_r, auto &other_task, bool &other_complete_r)
{
	assert(!complete_r);
	assert(!other_complete_r);

	co_await task;

	assert(!complete_r);
	assert(!other_complete_r);

	other_task = Waiter(task, other_complete_r);
	assert(!other_complete_r);

	assert(!complete_r);
	complete_r = true;
}

/**
 * One resumed task adds another waiter.
 */
TEST(MultiResume, AwaitInTask)
{
	Co::MultiResume m;
	m.ResumeAll();

	std::array complete{false, false};
	std::array<Co::EagerTask<void>, 2> waiters;
	waiters[0] = AwaitOtherTaskWaiter(m, complete[0], waiters[1], complete[1]);

	EXPECT_FALSE(complete[0]);
	EXPECT_FALSE(complete[1]);

	/* waiters[0] schedules waiters[1] but this will not resume
           waiters[1]; it was added before ResumeAll() was invoked */
	m.ResumeAll();
	EXPECT_TRUE(complete[0]);
	EXPECT_FALSE(complete[1]);

	/* now resume waiters[1] */
	m.ResumeAll();
	EXPECT_TRUE(complete[0]);
	EXPECT_TRUE(complete[1]);
}
