// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "spawn/CgroupState.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "util/PrintException.hxx"

#include <cstdio>

struct Usage {};

int
main(int, char **) noexcept
try {
	const auto state = CgroupState::FromProcess();
	if (!state.IsEnabled())
		throw "cgroups disabled";

	printf("group_path = '%s'\n", state.group_path.c_str());

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
