/*
 * Copyright 2019-2022 CM4all GmbH
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

#include "spawn/Direct.hxx"
#include "spawn/Prepared.hxx"
#include "spawn/CgroupState.hxx"
#include "spawn/CgroupKill.hxx"
#include "event/Loop.hxx"
#include "util/ConstBuffer.hxx"
#include "util/PrintException.hxx"
#include "util/StringCompare.hxx"
#include "AllocatorPtr.hxx"

#include <stdio.h>

struct Usage {};

class MyCgroupKillHandler final : public CgroupKillHandler {
	std::exception_ptr error;

public:
	void CheckRethrow() {
		if (error)
			std::rethrow_exception(error);
	}

	void OnCgroupKill() noexcept override {
	}

	void OnCgroupKillError(std::exception_ptr _error) noexcept override {
		error = std::move(_error);
	}
};

int
main(int argc, char **argv)
try {
	ConstBuffer<const char *> args(argv + 1, argc - 1);

	if (args.size < 2)
		throw Usage{};

	const char *scope = args.shift();
	const char *name = args.shift();
	const char *session = args.empty() ? nullptr : args.shift();

	if (!args.empty())
		throw Usage{};

	const auto cgroup_state = CgroupState::FromProcess(0, scope);

	EventLoop event_loop;

	MyCgroupKillHandler handler;
	CgroupKill cgroup_kill(event_loop, cgroup_state, name, session,
			       handler);

	event_loop.Dispatch();

	handler.CheckRethrow();
	return EXIT_SUCCESS;
} catch (Usage) {
	fprintf(stderr, "Usage: KillCgroup"
		" SCOPE NAME [SESSION]"
		"\n");
	return EXIT_FAILURE;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
