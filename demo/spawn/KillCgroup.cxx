// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

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

	event_loop.Run();

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
