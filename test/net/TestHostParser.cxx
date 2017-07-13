#include "net/HostParser.hxx"

#include <gtest/gtest.h>

TEST(HostParserTest, Name)
{
    const auto input = "foo";
    const auto eh = ExtractHost(input);
    ASSERT_EQ(eh.host.data, input);
    ASSERT_EQ(eh.host.size, 3);
    ASSERT_EQ(eh.end, input + 3);
}

TEST(HostParserTest, NamePort)
{
    const auto input = "foo:80";
    const auto eh = ExtractHost(input);
    ASSERT_EQ(eh.host.data, input);
    ASSERT_EQ(eh.host.size, 3);
    ASSERT_EQ(eh.end, input + 3);
}

TEST(HostParserTest, IPv4)
{
    const auto input = "1.2.3.4";
    const auto eh = ExtractHost(input);
    ASSERT_EQ(eh.host.data, input);
    ASSERT_EQ(eh.host.size, 7);
    ASSERT_EQ(eh.end, input + 7);
}

TEST(HostParserTest, IPv6Wildcard)
{
    const auto input = "::";
    const auto eh = ExtractHost(input);
    ASSERT_EQ(eh.host.data, input);
    ASSERT_EQ(eh.host.size, 2);
    ASSERT_EQ(eh.end, input + 2);
}

TEST(HostParserTest, IPv6Local)
{
    const auto input = "::1";
    const auto eh = ExtractHost(input);
    ASSERT_EQ(eh.host.data, input);
    ASSERT_EQ(eh.host.size, 3);
    ASSERT_EQ(eh.end, input + 3);
}

TEST(HostParserTest, IPv6Static)
{
    const auto input = "2001:affe::";
    const auto eh = ExtractHost(input);
    ASSERT_EQ(eh.host.data, input);
    ASSERT_EQ(eh.host.size, 11);
    ASSERT_EQ(eh.end, input + 11);
}

TEST(HostParserTest, IPv6StaticPort)
{
    const auto input = "[2001:affe::]:80";
    const auto eh = ExtractHost(input);
    ASSERT_EQ(eh.host.data, input + 1);
    ASSERT_EQ(eh.host.size, 11);
    ASSERT_EQ(eh.end, input + 13);
}
