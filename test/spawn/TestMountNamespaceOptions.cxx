// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "spawn/MountNamespaceOptions.hxx"
#include "spawn/Mount.hxx"

#include <gtest/gtest.h>

TEST(MountNamespaceOptions, GetJailedHome)
{
	MountNamespaceOptions options;
	options.home = "/mnt/data/foo/bar";

	EXPECT_STREQ(options.GetJailedHome(), options.home);

	Mount mount_home{options.home + 1, "/home"};
	options.mounts.push_front(mount_home);

	EXPECT_STREQ(options.GetJailedHome(), mount_home.target);
}
