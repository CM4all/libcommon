/*
 * Copyright 2007-2021 CM4all GmbH
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

#include "net/Parser.hxx"
#include "net/ToString.hxx"
#include "net/AllocatedSocketAddress.hxx"

#include <gtest/gtest.h>

TEST(SocketAddressStringTest, Basic)
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
        ASSERT_TRUE(ToString(buffer, sizeof(buffer), address));
        const char *out = i.out != nullptr ? i.out : i.in;
        ASSERT_STREQ(buffer, out);

        ASSERT_TRUE(HostToString(buffer, sizeof(buffer), address));
        const char *host = i.host != nullptr ? i.host : i.in;
        ASSERT_STREQ(buffer, host);
    }
}
