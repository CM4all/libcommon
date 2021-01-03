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

#include "http/List.hxx"
#include "util/StringView.hxx"

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
