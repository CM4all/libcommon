// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "net/Parser.hxx"
#include "net/FormatAddress.hxx"
#include "net/AllocatedSocketAddress.hxx"

#include <gtest/gtest.h>

TEST(FormatAddress, Basic)
{
    static constexpr struct {
        const char *in, *out, *host;
    } tests[] = {
        { "/local.socket", nullptr, nullptr },
        { "@abstract.socket", nullptr, nullptr },
        { "127.0.0.1:1234", nullptr, "127.0.0.1" },
        { "::1", "[::1]:80", nullptr },
        { "[::1]:1234", nullptr, "::1" },
        { "2001:affe::", "[2001:affe::]:80", nullptr },
        { "[2001:affe::]:1234", nullptr, "2001:affe::" },
    };

    for (const auto &i : tests) {
        const auto address = ParseSocketAddress(i.in, 80, false);

        char buffer[256];
        ASSERT_TRUE(ToString(std::span{buffer}, address));
        const char *out = i.out != nullptr ? i.out : i.in;
        ASSERT_STREQ(buffer, out);

        ASSERT_TRUE(HostToString(std::span{buffer}, address));
        const char *host = i.host != nullptr ? i.host : i.in;
        ASSERT_STREQ(buffer, host);
    }
}
