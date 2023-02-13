// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "http/List.hxx"

#include <gtest/gtest.h>

TEST(HttpListTest, Contains)
{
    ASSERT_TRUE(http_list_contains("foo", "foo"));
    ASSERT_TRUE(!http_list_contains("foo", "bar"));
    ASSERT_TRUE(http_list_contains("foo,bar", "bar"));
    ASSERT_TRUE(http_list_contains("bar,foo", "bar"));
    ASSERT_TRUE(!http_list_contains("bar,foo", "bart"));
    ASSERT_TRUE(!http_list_contains("foo,bar", "bart"));
    ASSERT_TRUE(http_list_contains("bar,foo", "\"bar\""));
    ASSERT_TRUE(http_list_contains("\"bar\",\"foo\"", "\"bar\""));
    ASSERT_TRUE(http_list_contains("\"bar\",\"foo\"", "bar"));
}
