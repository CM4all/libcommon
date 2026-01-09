// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "event/co/Timeout.hxx"
#include "event/co/Yield.hxx"
#include "event/Loop.hxx"
#include "co/Task.hxx"
#include "co/InvokeTask.hxx"

#include <gtest/gtest.h>

/**
 * A task that is never ready and thus never resumes the continuation.
 */
class NeverReady {
	struct Awaitable {
		[[nodiscard]]
		constexpr bool await_ready() const noexcept {
			return false;
		}

		constexpr void await_suspend([[maybe_unused]] std::coroutine_handle<> continuation) noexcept {
		}

		constexpr void await_resume() noexcept {
		}
	};

public:
	constexpr Awaitable operator co_await() const noexcept {
		return {};
	}
};

struct Completion {
	std::exception_ptr error;
	bool done = false;

	void Callback(std::exception_ptr &&_error) noexcept {
		assert(!done);
		assert(!error);

		error = std::move(_error);
		done = true;
	}

	void Start(Co::InvokeTask &invoke) noexcept {
		assert(invoke);
		invoke.Start(BIND_THIS_METHOD(Callback));
	}
};

static Co::Task<void>
IncTask(int &i) noexcept
{
	++i;
	co_return;
}

static Co::Task<void>
IncThrowTask(int &i)
{
	++i;
	throw i;
	co_return;
}

static Co::Task<void>
YieldIncTask(EventLoop &event_loop, int &i) noexcept
{
	++i;
	co_await Co::Yield{event_loop};
	++i;
	co_return;
}

static Co::Task<void>
YieldIncThrowTask(EventLoop &event_loop, int &i) noexcept
{
	++i;
	co_await Co::Yield{event_loop};
	throw i;
}

template<typename T>
static Co::InvokeTask
InvokeWithTimeout(EventLoop &event_loop, Event::Duration timeout, T task)
{
	co_await Co::Timeout{event_loop, timeout, std::move(task)};
}

/**
 * Like InvokeWithTimeout(), but leave the Co::Timeout instance on the
 * stack so it is not automatically destructed, and then await
 * something that will never finish.  This verifies that the
 * Co::Timeout cancels the timer after the inner task finishes.
 */
template<typename T>
static Co::InvokeTask
InvokeWithTimeoutNeverDestruct(EventLoop &event_loop, Event::Duration _timeout, T task)
{
	Co::Timeout timeout{event_loop, _timeout, std::move(task)};
	co_await timeout;
	co_await NeverReady{};
}

/**
 * Finishes immediately.
 */
TEST(Timeout, Basic)
{
	EventLoop event_loop;

	int i = 0;
	auto invoke = InvokeWithTimeout(event_loop, {}, IncTask(i));
	ASSERT_TRUE(invoke);
	ASSERT_EQ(i, 0);

	Completion c;
	c.Start(invoke);

	ASSERT_FALSE(invoke);
	ASSERT_TRUE(c.done);
	ASSERT_FALSE(c.error);
	ASSERT_EQ(i, 1);

	event_loop.Run();

	ASSERT_FALSE(invoke);
	ASSERT_TRUE(c.done);
	ASSERT_FALSE(c.error);
	ASSERT_EQ(i, 1);
}

/**
 * Immediately throws an exception.
 */
TEST(Timeout, Throw)
{
	EventLoop event_loop;

	int i = 0;
	auto invoke = InvokeWithTimeout(event_loop, {}, IncThrowTask(i));
	ASSERT_TRUE(invoke);
	ASSERT_EQ(i, 0);

	Completion c;
	c.Start(invoke);

	ASSERT_FALSE(invoke);
	ASSERT_TRUE(c.done);
	ASSERT_TRUE(c.error);
	ASSERT_EQ(i, 1);

	event_loop.Run();

	ASSERT_FALSE(invoke);
	ASSERT_TRUE(c.done);
	ASSERT_TRUE(c.error);
	ASSERT_EQ(i, 1);
}

/**
 * Finishes after yielding once.
 */
TEST(Timeout, Yield)
{
	EventLoop event_loop;

	int i = 0;
	auto invoke = InvokeWithTimeout(event_loop, std::chrono::hours{1}, YieldIncTask(event_loop, i));
	ASSERT_TRUE(invoke);
	ASSERT_EQ(i, 0);

	Completion c;
	c.Start(invoke);

	ASSERT_TRUE(invoke);
	ASSERT_FALSE(c.done);
	ASSERT_FALSE(c.error);
	ASSERT_EQ(i, 1);

	event_loop.Run();

	ASSERT_FALSE(invoke);
	ASSERT_TRUE(c.done);
	ASSERT_FALSE(c.error);
	ASSERT_EQ(i, 2);

	event_loop.Run();

	ASSERT_FALSE(invoke);
	ASSERT_TRUE(c.done);
	ASSERT_FALSE(c.error);
	ASSERT_EQ(i, 2);
}

/**
 * Throws after yielding once.
 */
TEST(Timeout, YieldThrow)
{
	EventLoop event_loop;

	int i = 0;
	auto invoke = InvokeWithTimeout(event_loop, std::chrono::hours{1}, YieldIncThrowTask(event_loop, i));
	ASSERT_TRUE(invoke);
	ASSERT_EQ(i, 0);

	Completion c;
	c.Start(invoke);

	ASSERT_TRUE(invoke);
	ASSERT_FALSE(c.done);
	ASSERT_FALSE(c.error);
	ASSERT_EQ(i, 1);

	event_loop.Run();

	ASSERT_FALSE(invoke);
	ASSERT_TRUE(c.done);
	ASSERT_TRUE(c.error);
	ASSERT_EQ(i, 1);

	event_loop.Run();

	ASSERT_FALSE(invoke);
	ASSERT_TRUE(c.done);
	ASSERT_TRUE(c.error);
	ASSERT_EQ(i, 1);
}

/**
 * Never finishes, times out.
 */
TEST(Timeout, Timeout)
{
	EventLoop event_loop;

	auto invoke = InvokeWithTimeout(event_loop, {}, NeverReady{});
	ASSERT_TRUE(invoke);

	Completion c;
	c.Start(invoke);

	ASSERT_TRUE(invoke);
	ASSERT_FALSE(c.done);
	ASSERT_FALSE(c.error);

	event_loop.Run();

	ASSERT_FALSE(invoke);
	ASSERT_TRUE(c.done);
	ASSERT_TRUE(c.error);
}

TEST(Timeout, NoDestruct)
{
	EventLoop event_loop;

	int i = 0;
	auto invoke = InvokeWithTimeoutNeverDestruct(event_loop, {}, IncTask(i));
	ASSERT_TRUE(invoke);

	Completion c;
	c.Start(invoke);

	ASSERT_TRUE(invoke);
	ASSERT_FALSE(c.done);
	ASSERT_FALSE(c.error);
	ASSERT_EQ(i, 1);

	event_loop.Run();

	ASSERT_TRUE(invoke);
	ASSERT_FALSE(c.done);
	ASSERT_FALSE(c.error);
	ASSERT_EQ(i, 1);
}
