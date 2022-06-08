/*
 * Copyright 2007-2022 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "co/Cache.hxx"
#include "co/Task.hxx"
#include "co/InvokeTask.hxx"
#include "co/Sleep.hxx"
#include "event/Loop.hxx"

#include <gtest/gtest.h>

#include <cassert>

/*
static Co::Task<int>
TheLazyTask(int i)
{
	co_return i;
}
*/

static Co::EagerTask<int>
TheEagerTask(int i)
{
	//co_return co_await TheLazyTask(i);
	co_return i;
}

static Co::InvokeTask
RunEagerTask(int i, int &result)
{
	result = co_await TheEagerTask(i);
}

TEST(Task, EagerTask)
{
	int value = -1;
	std::exception_ptr error;

	auto task = RunEagerTask(42, value);
	task.Start({&error, [](void *error_p, std::exception_ptr _error) noexcept {
		auto &error_r = *(std::exception_ptr *)error_p;
		error_r = std::move(_error);
	}});

	ASSERT_FALSE(error);
	ASSERT_EQ(value, 42);
}
