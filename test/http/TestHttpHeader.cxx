// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "http/HeaderName.hxx"
#include "http/HeaderValue.hxx"

#include <gtest/gtest.h>

using std::string_view_literals::operator""sv;

static constexpr const char *valid_http_headers[] = {
	"0",
	"x",
	"x-",
	"foo",
	"foo-bar",
	"!#$%&'*+-.^_`|~",
};

static constexpr const char *invalid_http_headers[] = {
	"",
	"foo bar",
	"foo\n-bar",
	"foo\r-bar",
	"foo\t-bar",
	"foo\x7f-bar",
	"foo\x80-bar",
	"foo\xff-bar",
};

static constexpr std::string_view invalid_http_headers2[] = {
	"foo\0bar"sv,
};

TEST(HttpHeader, NameValid)
{
	for (const char *i : valid_http_headers)
		EXPECT_TRUE(http_header_name_valid(i));

	for (const char *i : invalid_http_headers)
		EXPECT_FALSE(http_header_name_valid(i));

	for (std::string_view i : valid_http_headers)
		EXPECT_TRUE(http_header_name_valid(i));

	for (std::string_view i : invalid_http_headers)
		EXPECT_FALSE(http_header_name_valid(i));

	for (std::string_view i : invalid_http_headers2)
		EXPECT_FALSE(http_header_name_valid(i));
}

TEST(HttpHeader, ValueValid)
{
	EXPECT_TRUE(IsValidHttpHeaderValue(""sv));
	EXPECT_TRUE(IsValidHttpHeaderValue("foo-bar"sv));
	EXPECT_TRUE(IsValidHttpHeaderValue("foo bar"sv));
	EXPECT_TRUE(IsValidHttpHeaderValue("foo\tbar"sv));
	EXPECT_FALSE(IsValidHttpHeaderValue("foo\rbar"sv));
	EXPECT_FALSE(IsValidHttpHeaderValue("foo\0"sv));
	EXPECT_FALSE(IsValidHttpHeaderValue("\0"sv));
}
