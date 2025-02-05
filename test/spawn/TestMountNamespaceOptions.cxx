// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "spawn/MountNamespaceOptions.hxx"
#include "spawn/Mount.hxx"
#include "AllocatorPtr.hxx"

#include <gtest/gtest.h>

TEST(MountNamespaceOptions, ToContainerPath)
{
	Allocator alloc;

	MountNamespaceOptions options;
	options.home = "/mnt/data/foo/bar";

	EXPECT_STREQ(options.ToContainerPath(alloc, options.home), options.home);

	options.mount_root_tmpfs = true;
	EXPECT_EQ(options.ToContainerPath(alloc, options.home), nullptr);

	Mount mount_bar{options.home + 1, "/home/www"};
	options.mounts.push_front(mount_bar);
	EXPECT_STREQ(options.ToContainerPath(alloc, options.home), mount_bar.target);

	options.mounts.clear();
	Mount mount_foo{"mnt/data/foo", "/home"};
	options.mounts.push_front(mount_foo);
	EXPECT_STREQ(options.ToContainerPath(alloc, options.home), "/home/bar");

	options.mounts.clear();
	Mount mount_data{"mnt/data", "/home"};
	options.mounts.push_front(mount_data);
	EXPECT_STREQ(options.ToContainerPath(alloc, options.home), "/home/foo/bar");

	options.mounts.clear();
	Mount mount_mismatch{"mnt/data/mismatch", "/home"};
	options.mounts.push_front(mount_mismatch);
	EXPECT_EQ(options.ToContainerPath(alloc, options.home), nullptr);

	options.mounts.clear();
	Mount mount_mismatch2{"mnt/data/foo/bar/abc", "/home"};
	options.mounts.push_front(mount_mismatch2);
	EXPECT_EQ(options.ToContainerPath(alloc, options.home), nullptr);

	options.mounts.clear();
	Mount mount_mismatch3{"mnt/da", "/home"};
	options.mounts.push_front(mount_mismatch3);
	EXPECT_EQ(options.ToContainerPath(alloc, options.home), nullptr);
}
