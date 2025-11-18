// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "co/Task.hxx"
#include "co/InvokeTask.hxx"

#include <gtest/gtest.h>

#include <cassert>

static Co::OptionalTask<int>
TheCoroutine(int i)
{
	co_return i;
}

static Co::OptionalTask<int>
NoOptionalTask()
{
	return {};
}

static Co::OptionalTask<int>
MakeOptionalTask(int i)
{
	return TheCoroutine(i);
}

static Co::InvokeTask
InvokeTask(auto &result, auto &&task)
{
	result = co_await task;
}

TEST(OptionalTask, WithTask)
{
	int value = -1;
	std::exception_ptr error;

	auto task = InvokeTask(value, MakeOptionalTask(42));
	task.Start({&error, [](void *error_p, std::exception_ptr &&_error) noexcept {
		auto &error_r = *(std::exception_ptr *)error_p;
		error_r = std::move(_error);
	}});

	ASSERT_FALSE(error);
	ASSERT_EQ(value, 42);
}

TEST(OptionalTask, NoTask)
{
	int value = -1;
	std::exception_ptr error;

	auto task = InvokeTask(value, NoOptionalTask());
	task.Start({&error, [](void *error_p, std::exception_ptr &&_error) noexcept {
		auto &error_r = *(std::exception_ptr *)error_p;
		error_r = std::move(_error);
	}});

	ASSERT_FALSE(error);
	ASSERT_EQ(value, 0);
}
