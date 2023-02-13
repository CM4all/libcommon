// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "util/CRC32.hxx"

#include <gtest/gtest.h>

TEST(CRC32, Basic)
{
	EXPECT_EQ(CRC32(std::as_bytes(std::span{"123456789", 9})),
		  0xcbf43926);
}
