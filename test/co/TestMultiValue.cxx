// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "co/MultiValue.hxx"
#include "co/Task.hxx"

#include <gtest/gtest.h>

template<typename T>
static Co::EagerTask<void>
Waiter(Co::MultiValue<T> &task, std::optional<T> &value_r)
{
	assert(!value_r);
	value_r = co_await task;
}

TEST(MultiValue, Nothing)
{
	[[maybe_unused]]
	Co::MultiValue<int> m;
}

TEST(MultiValue, ReadyNone)
{
	Co::MultiValue<int> m;
	m.SetReady(42);
}

TEST(MultiValue, ReadyEarly)
{
	Co::MultiValue<int> m;
	m.SetReady(42);

	std::optional<int> value;
	auto w = Waiter(m, value);

	EXPECT_TRUE(value);
	EXPECT_EQ(*value, 42);
}

TEST(MultiValue, ReadyLate)
{
	Co::MultiValue<int> m;

	std::optional<int> value;
	auto w = Waiter(m, value);

	EXPECT_FALSE(value);

	m.SetReady(42);
	EXPECT_TRUE(value);
	EXPECT_EQ(*value, 42);
}

TEST(MultiValue, ResumeFour)
{
	Co::MultiValue<int> m;
	std::optional<int> values[4];

	auto w0 = Waiter(m, values[0]);
	auto w1 = Waiter(m, values[1]);

	EXPECT_FALSE(values[0]);
	EXPECT_FALSE(values[1]);
	EXPECT_FALSE(values[2]);
	EXPECT_FALSE(values[3]);

	m.SetReady(42);

	EXPECT_TRUE(values[0]);
	EXPECT_TRUE(values[1]);
	EXPECT_FALSE(values[2]);
	EXPECT_FALSE(values[3]);

	EXPECT_EQ(*values[0], 42);
	EXPECT_EQ(*values[1], 42);

	auto w2 = Waiter(m, values[2]);
	auto w3 = Waiter(m, values[3]);

	EXPECT_TRUE(values[0]);
	EXPECT_TRUE(values[1]);
	EXPECT_TRUE(values[2]);
	EXPECT_TRUE(values[3]);

	EXPECT_EQ(*values[0], 42);
	EXPECT_EQ(*values[1], 42);
	EXPECT_EQ(*values[2], 42);
	EXPECT_EQ(*values[3], 42);
}

TEST(MultiValue, Cancel)
{
	Co::MultiValue<int> m;
	std::optional<int> values[6];

	auto w0 = Waiter(m, values[0]);
	auto w1 = Waiter(m, values[1]);

	EXPECT_FALSE(values[0]);
	EXPECT_FALSE(values[1]);
	EXPECT_FALSE(values[2]);
	EXPECT_FALSE(values[3]);
	EXPECT_FALSE(values[4]);
	EXPECT_FALSE(values[5]);

	w0 = {};
	w1 = {};

	EXPECT_FALSE(values[0]);
	EXPECT_FALSE(values[1]);
	EXPECT_FALSE(values[2]);
	EXPECT_FALSE(values[3]);
	EXPECT_FALSE(values[4]);
	EXPECT_FALSE(values[5]);

	auto w2 = Waiter(m, values[2]);
	auto w3 = Waiter(m, values[3]);

	EXPECT_FALSE(values[0]);
	EXPECT_FALSE(values[1]);
	EXPECT_FALSE(values[2]);
	EXPECT_FALSE(values[3]);
	EXPECT_FALSE(values[4]);
	EXPECT_FALSE(values[5]);

	w3 = {};

	EXPECT_FALSE(values[0]);
	EXPECT_FALSE(values[1]);
	EXPECT_FALSE(values[2]);
	EXPECT_FALSE(values[3]);
	EXPECT_FALSE(values[4]);
	EXPECT_FALSE(values[5]);

	m.SetReady(42);

	EXPECT_FALSE(values[0]);
	EXPECT_FALSE(values[1]);
	EXPECT_TRUE(values[2]);
	EXPECT_FALSE(values[3]);
	EXPECT_FALSE(values[4]);
	EXPECT_FALSE(values[5]);

	EXPECT_EQ(*values[2], 42);

	auto w4 = Waiter(m, values[4]);
	auto w5 = Waiter(m, values[5]);

	EXPECT_FALSE(values[0]);
	EXPECT_FALSE(values[1]);
	EXPECT_TRUE(values[2]);
	EXPECT_FALSE(values[3]);
	EXPECT_TRUE(values[4]);
	EXPECT_TRUE(values[5]);

	EXPECT_EQ(*values[2], 42);
	EXPECT_EQ(*values[4], 42);
	EXPECT_EQ(*values[5], 42);
}

template<typename T>
static Co::EagerTask<void>
CancelOtherTaskWaiter(Co::MultiValue<T> &task, std::optional<T> &value_r,
		      auto &cancel_task)
{
	assert(!value_r);
	auto value = co_await task;

	cancel_task = {};
	assert(!value_r);

	value_r.emplace(std::move(value));
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
TEST(MultiValue, CancelInTask)
{
	Co::MultiValue<int> m;

	std::optional<int> values[2];
	std::array<Co::EagerTask<void>, 2> waiters;
	waiters[0] = CancelOtherTaskWaiter(m, values[0], waiters[1]);
	waiters[1] = Waiter(m, values[1]);

	EXPECT_FALSE(values[0]);
	EXPECT_FALSE(values[1]);

	m.SetReady(42);
	EXPECT_TRUE(values[0]);
	EXPECT_FALSE(values[1]);

	EXPECT_EQ(*values[0], 42);
}

template<typename T>
static Co::EagerTask<void>
AwaitOtherTaskWaiter(Co::MultiValue<T> &task, std::optional<T> &value_r,
		     auto &other_task, std::optional<T> &other_value_r)
{
	assert(!value_r);
	assert(!other_value_r);

	value_r = co_await task;

	assert(!other_value_r);

	other_task = Waiter(task, other_value_r);
	assert(other_value_r);
}

/**
 * One resumed task adds another waiter.
 */
TEST(MultiValue, AwaitInTask)
{
	Co::MultiValue<int> m;

	std::optional<int> values[2];
	std::array<Co::EagerTask<void>, 2> waiters;
	waiters[0] = AwaitOtherTaskWaiter(m, values[0], waiters[1], values[1]);

	EXPECT_FALSE(values[0]);
	EXPECT_FALSE(values[1]);

	m.SetReady(42);
	EXPECT_TRUE(values[0]);
	EXPECT_TRUE(values[1]);

	EXPECT_EQ(*values[0], 42);
	EXPECT_EQ(*values[1], 42);
}
