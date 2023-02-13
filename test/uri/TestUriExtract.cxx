// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "uri/Extract.hxx"

#include <gtest/gtest.h>

#include <string.h>
#include <stdlib.h>

static constexpr struct UriTests {
	const char *uri;
	const char *host_and_port;
	const char *path;
	const char *query_string;
} uri_tests[] = {
	{ "http://foo/bar", "foo", "/bar", nullptr },
	{ "https://foo/bar", "foo", "/bar", nullptr },
	{ "http://foo:8080/bar", "foo:8080", "/bar", nullptr },
	{ "http://foo", "foo", nullptr, nullptr },
	{ "http://foo/bar?a=b", "foo", "/bar?a=b", "a=b" },
	{ "whatever-scheme://foo/bar?a=b", "foo", "/bar?a=b", "a=b" },
	{ "//foo/bar", "foo", "/bar", nullptr },
	{ "//foo", "foo", nullptr, nullptr },
	{ "/bar?a=b", nullptr, "/bar?a=b", "a=b" },
	{ "bar?a=b", nullptr, "bar?a=b", "a=b" },
};

TEST(UriExtractTest, HostAndPort)
{
	for (auto i : uri_tests) {
		auto result = UriHostAndPort(i.uri);
		if (i.host_and_port == nullptr) {
			ASSERT_EQ(result.data(), nullptr);
			ASSERT_EQ(result.size(), size_t(0));
		} else {
			ASSERT_EQ(result, std::string_view{i.host_and_port});
		}
	}
}

TEST(UriExtractTest, Path)
{
	for (auto i : uri_tests) {
		auto result = UriPathQueryFragment(i.uri);
		if (i.path == nullptr)
			ASSERT_EQ(i.path, result);
		else
			ASSERT_EQ(strcmp(i.path, result), 0);
	}
}

TEST(UriExtractTest, QueryString)
{
	for (auto i : uri_tests) {
		auto result = UriQuery(i.uri);
		if (i.query_string == nullptr)
			ASSERT_EQ(i.query_string, result);
		else
			ASSERT_EQ(strcmp(i.query_string, result), 0);
	}
}
