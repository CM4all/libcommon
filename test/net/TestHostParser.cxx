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

#include "net/HostParser.hxx"

#include <gtest/gtest.h>

TEST(HostParserTest, Name)
{
    const auto input = "foo";
    const auto eh = ExtractHost(input);
    ASSERT_EQ(eh.host.data, input);
    ASSERT_EQ(eh.host.size, 3u);
    ASSERT_EQ(eh.end, input + 3);
}

TEST(HostParserTest, NamePort)
{
    const auto input = "foo:80";
    const auto eh = ExtractHost(input);
    ASSERT_EQ(eh.host.data, input);
    ASSERT_EQ(eh.host.size, 3u);
    ASSERT_EQ(eh.end, input + 3);
}

TEST(HostParserTest, IPv4)
{
    const auto input = "1.2.3.4";
    const auto eh = ExtractHost(input);
    ASSERT_EQ(eh.host.data, input);
    ASSERT_EQ(eh.host.size, 7u);
    ASSERT_EQ(eh.end, input + 7);
}

TEST(HostParserTest, IPv6Wildcard)
{
    const auto input = "::";
    const auto eh = ExtractHost(input);
    ASSERT_EQ(eh.host.data, input);
    ASSERT_EQ(eh.host.size, 2u);
    ASSERT_EQ(eh.end, input + 2);
}

TEST(HostParserTest, IPv6Local)
{
    const auto input = "::1";
    const auto eh = ExtractHost(input);
    ASSERT_EQ(eh.host.data, input);
    ASSERT_EQ(eh.host.size, 3u);
    ASSERT_EQ(eh.end, input + 3);
}

TEST(HostParserTest, IPv6Static)
{
    const auto input = "2001:affe::";
    const auto eh = ExtractHost(input);
    ASSERT_EQ(eh.host.data, input);
    ASSERT_EQ(eh.host.size, 11u);
    ASSERT_EQ(eh.end, input + 11);
}

TEST(HostParserTest, IPv6StaticPort)
{
    const auto input = "[2001:affe::]:80";
    const auto eh = ExtractHost(input);
    ASSERT_EQ(eh.host.data, input + 1);
    ASSERT_EQ(eh.host.size, 11u);
    ASSERT_EQ(eh.end, input + 13);
}

TEST(HostParserTest, IPv6Scope)
{
    const auto input = "2001:affe::%eth0";
    const auto eh = ExtractHost(input);
    ASSERT_EQ(eh.host.data, input);
    ASSERT_EQ(eh.host.size, 16u);
    ASSERT_EQ(eh.end, input + 16);
}

TEST(HostParserTest, IPv6ScopePort)
{
    const auto input = "[2001:affe::%eth0]:80";
    const auto eh = ExtractHost(input);
    ASSERT_EQ(eh.host.data, input + 1);
    ASSERT_EQ(eh.host.size, 16u);
    ASSERT_EQ(eh.end, input + 18);
}
