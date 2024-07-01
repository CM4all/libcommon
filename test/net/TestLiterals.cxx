// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "net/Literals.hxx"
#include "net/ToString.hxx"

#include <gtest/gtest.h>

#ifndef _WIN32
#include <arpa/inet.h>
#endif

static std::string
ToString(const struct in_addr &a)
{
#ifdef _WIN32
	/* on mingw32, the parameter is non-const (PVOID) */
	const auto p = const_cast<struct in_addr *>(&a);
#else
	const auto p = &a;
#endif

	char buffer[256];
	const char *result = inet_ntop(AF_INET, p, buffer, sizeof(buffer));
	if (result == nullptr)
		throw std::runtime_error("inet_ntop() failed");
	return result;
}

TEST(Literals, IPv4)
{
	static constexpr auto a = "11.22.33.44:1234"_ipv4;
	EXPECT_EQ(ToString(a.GetAddress()), "11.22.33.44");
	EXPECT_EQ(a.GetPort(), 1234);

	static constexpr auto b = "11.22.33.44"_ipv4;
	EXPECT_EQ(ToString(b.GetAddress()), "11.22.33.44");
	EXPECT_EQ(b.GetPort(), 0);

	static constexpr auto c = "11.22.33.44:0"_ipv4;
	EXPECT_EQ(ToString(c.GetAddress()), "11.22.33.44");
	EXPECT_EQ(c.GetPort(), 0);

	EXPECT_THROW((void)""_ipv4, std::invalid_argument);
	EXPECT_THROW((void)"a.1.1.1"_ipv4, std::invalid_argument);
	EXPECT_THROW((void)"1.1.1.1.1"_ipv4, std::invalid_argument);
	EXPECT_THROW((void)"1.1.1.1:1.1"_ipv4, std::invalid_argument);
	EXPECT_THROW((void)"11.22.33.256"_ipv4, std::overflow_error);
	EXPECT_THROW((void)"11.22.33.44:65536"_ipv4, std::overflow_error);
}
