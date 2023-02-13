// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "net/HostParser.hxx"

#include <gtest/gtest.h>

TEST(HostParserTest, Name)
{
    const auto input = "foo";
    const auto eh = ExtractHost(input);
    ASSERT_EQ(eh.host.data(), input);
    ASSERT_EQ(eh.host.size(), 3u);
    ASSERT_EQ(eh.end, input + 3);
}

TEST(HostParserTest, NamePort)
{
    const auto input = "foo:80";
    const auto eh = ExtractHost(input);
    ASSERT_EQ(eh.host.data(), input);
    ASSERT_EQ(eh.host.size(), 3u);
    ASSERT_EQ(eh.end, input + 3);
}

TEST(HostParserTest, IPv4)
{
    const auto input = "1.2.3.4";
    const auto eh = ExtractHost(input);
    ASSERT_EQ(eh.host.data(), input);
    ASSERT_EQ(eh.host.size(), 7u);
    ASSERT_EQ(eh.end, input + 7);
}

TEST(HostParserTest, IPv6Wildcard)
{
    const auto input = "::";
    const auto eh = ExtractHost(input);
    ASSERT_EQ(eh.host.data(), input);
    ASSERT_EQ(eh.host.size(), 2u);
    ASSERT_EQ(eh.end, input + 2);
}

TEST(HostParserTest, IPv6Local)
{
    const auto input = "::1";
    const auto eh = ExtractHost(input);
    ASSERT_EQ(eh.host.data(), input);
    ASSERT_EQ(eh.host.size(), 3u);
    ASSERT_EQ(eh.end, input + 3);
}

TEST(HostParserTest, IPv6Static)
{
    const auto input = "2001:affe::";
    const auto eh = ExtractHost(input);
    ASSERT_EQ(eh.host.data(), input);
    ASSERT_EQ(eh.host.size(), 11u);
    ASSERT_EQ(eh.end, input + 11);
}

TEST(HostParserTest, IPv6StaticPort)
{
    const auto input = "[2001:affe::]:80";
    const auto eh = ExtractHost(input);
    ASSERT_EQ(eh.host.data(), input + 1);
    ASSERT_EQ(eh.host.size(), 11u);
    ASSERT_EQ(eh.end, input + 13);
}

TEST(HostParserTest, IPv6Scope)
{
    const auto input = "2001:affe::%eth0";
    const auto eh = ExtractHost(input);
    ASSERT_EQ(eh.host.data(), input);
    ASSERT_EQ(eh.host.size(), 16u);
    ASSERT_EQ(eh.end, input + 16);
}

TEST(HostParserTest, IPv6ScopePort)
{
    const auto input = "[2001:affe::%eth0]:80";
    const auto eh = ExtractHost(input);
    ASSERT_EQ(eh.host.data(), input + 1);
    ASSERT_EQ(eh.host.size(), 16u);
    ASSERT_EQ(eh.end, input + 18);
}
