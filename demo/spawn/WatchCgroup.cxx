// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "spawn/Direct.hxx"
#include "spawn/Prepared.hxx"
#include "spawn/CgroupState.hxx"
#include "spawn/CgroupMemoryWatch.hxx"
#include "event/Loop.hxx"
#include "util/PrintException.hxx"
#include "util/StringCompare.hxx"
#include "AllocatorPtr.hxx"

#include <fmt/core.h>

#include <span>

struct Usage {};

class MyCgroupWatch {
	CgroupMemoryWatch watch;

public:
	MyCgroupWatch(EventLoop &event_loop,
		      FileDescriptor group_fd)
		:watch(event_loop, group_fd,
		       BIND_THIS_METHOD(OnCgroupWatch)) {}

private:
	void OnCgroupWatch(uint64_t value) noexcept {
		fmt::print("{}\n", value);
	}
};

int
main(int argc, char **argv)
try {
	std::span<const char *const> args{argv + 1, static_cast<std::size_t>(argc - 1)};

	if (args.size() != 1)
		throw Usage{};

	const char *scope = args.front();
	args = args.subspan(1);

	if (!args.empty())
		throw Usage{};

	const auto cgroup_state = CgroupState::FromProcess(0, scope);

	EventLoop event_loop;

	MyCgroupWatch watch{event_loop, cgroup_state.group_fd};

	event_loop.Run();

	return EXIT_SUCCESS;
} catch (Usage) {
	fmt::print(stderr, "Usage: WatchCgroup SCOPE\n");
	return EXIT_FAILURE;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
