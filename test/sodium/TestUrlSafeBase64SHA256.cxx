// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "lib/sodium/UrlSafeBase64SHA256.hxx"

#include <gtest/gtest.h>

TEST(TestUrlSafeBase64SHA256, Empty)
{
	const auto b64 = UrlSafeBase64SHA256("");
	EXPECT_STREQ(b64, "47DEQpj8HBSa-_TImW-5JCeuQeRkm5NMpJWZG3hSuFU");
}
