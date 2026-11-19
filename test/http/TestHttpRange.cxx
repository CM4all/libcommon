// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "http/Range.hxx"

#include <gtest/gtest.h>

using Type = HttpRangeRequest::Type;

static HttpRangeRequest
ParseRange(const char *p, uint_least64_t size) noexcept
{
	HttpRangeRequest r{size};
	r.ParseRangeHeader(p);
	return r;
}

/**
 * The header selects the byte range [skip,end) of an entity of the
 * given size.
 */
static void
ExpectRange(const char *p, uint_least64_t size,
	    uint_least64_t skip, uint_least64_t end) noexcept
{
	SCOPED_TRACE(testing::Message() << '"' << p << "\" size=" << size);

	const auto r = ParseRange(p, size);
	EXPECT_EQ(r.type, Type::VALID);
	EXPECT_EQ(r.skip, skip);
	EXPECT_EQ(r.size, end);
}

/**
 * No part of the entity is covered by the header; the server shall
 * respond with status 416.
 */
static void
ExpectUnsatisfiable(const char *p, uint_least64_t size) noexcept
{
	SCOPED_TRACE(testing::Message() << '"' << p << "\" size=" << size);

	EXPECT_EQ(ParseRange(p, size).type, Type::INVALID);
}

/**
 * The header shall be ignored; the server shall respond with the
 * whole entity and status 200.
 */
static void
ExpectIgnored(const char *p, uint_least64_t size) noexcept
{
	SCOPED_TRACE(testing::Message() << '"' << p << "\" size=" << size);

	EXPECT_EQ(ParseRange(p, size).type, Type::NONE);
}

TEST(HttpRange, IntRange)
{
	ExpectRange("bytes=0-99", 100, 0, 100);
	ExpectRange("bytes=0-0", 100, 0, 1);
	ExpectRange("bytes=0-1", 100, 0, 2);
	ExpectRange("bytes=10-19", 100, 10, 20);
	ExpectRange("bytes=99-99", 100, 99, 100);
	ExpectRange("bytes=0-0", 1, 0, 1);
}

/**
 * An int-range without last-pos covers the remainder of the entity
 * (this is what "wget -c" sends).
 */
TEST(HttpRange, OpenEndedIntRange)
{
	ExpectRange("bytes=0-", 100, 0, 100);
	ExpectRange("bytes=1-", 100, 1, 100);
	ExpectRange("bytes=99-", 100, 99, 100);
}

/**
 * RFC 9110 14.1.2: "if the value is greater than or equal to the
 * current length of the representation data, the byte range is
 * interpreted as the remainder of the representation".
 */
TEST(HttpRange, LastPosPastEnd)
{
	ExpectRange("bytes=0-100", 100, 0, 100);
	ExpectRange("bytes=0-999", 100, 0, 100);
	ExpectRange("bytes=50-999", 100, 50, 100);
	ExpectRange("bytes=99-999", 100, 99, 100);
	ExpectRange("bytes=0-1", 1, 0, 1);
}

/**
 * RFC 9110 15.5.17: status 416 is for a range set which does not
 * overlap the entity at all, i.e. first-pos is past its end.
 */
TEST(HttpRange, Unsatisfiable)
{
	ExpectUnsatisfiable("bytes=100-", 100);
	ExpectUnsatisfiable("bytes=100-100", 100);
	ExpectUnsatisfiable("bytes=100-200", 100);
	ExpectUnsatisfiable("bytes=1000-", 100);
	ExpectUnsatisfiable("bytes=1-", 1);
}

TEST(HttpRange, SuffixRange)
{
	ExpectRange("bytes=-1", 100, 99, 100);
	ExpectRange("bytes=-10", 100, 90, 100);
	ExpectRange("bytes=-99", 100, 1, 100);
	ExpectRange("bytes=-100", 100, 0, 100);
}

/**
 * RFC 9110 14.1.2: "If the selected representation is shorter than
 * the specified suffix-length, the entire representation is used."
 */
TEST(HttpRange, SuffixRangeLongerThanEntity)
{
	ExpectRange("bytes=-101", 100, 0, 100);
	ExpectRange("bytes=-1000", 100, 0, 100);
	ExpectRange("bytes=-2", 1, 0, 1);
}

/**
 * A suffix-length of zero selects no byte at all; there is no
 * "Content-Range" which can express that, so the only possible
 * answer is 416.
 */
TEST(HttpRange, ZeroSuffixRange)
{
	ExpectUnsatisfiable("bytes=-0", 100);
	ExpectUnsatisfiable("bytes=-0", 1);
}

/**
 * RFC 9110 14.1.1: "An int-range is invalid if the last-pos value is
 * present and less than the first-pos."  An invalid range-spec is not
 * the same as an unsatisfiable one: the header shall be ignored.
 */
TEST(HttpRange, InvalidIntRange)
{
	ExpectIgnored("bytes=50-40", 100);
	ExpectIgnored("bytes=1-0", 100);
	ExpectIgnored("bytes=200-100", 100);
}

/**
 * This parser supports only one range per request; RFC 9110 14.2
 * allows us to ignore the header instead.
 */
TEST(HttpRange, MultipleRanges)
{
	ExpectIgnored("bytes=0-1,5-6", 100);
	ExpectIgnored("bytes=0-1, 5-6", 100);
	ExpectIgnored("bytes=-1,-2", 100);
}

/**
 * RFC 9110 14.2: "An origin server MUST ignore a Range header field
 * that contains a range unit it does not understand."
 */
TEST(HttpRange, UnknownUnit)
{
	ExpectIgnored("", 100);
	ExpectIgnored("bytes", 100);
	ExpectIgnored("bytes=", 100);
	ExpectIgnored("items=0-5", 100);
	ExpectIgnored("none", 100);
}

TEST(HttpRange, Garbage)
{
	ExpectIgnored("bytes=abc", 100);
	ExpectIgnored("bytes=-abc", 100);
	ExpectIgnored("bytes=0-abc", 100);
	ExpectIgnored("bytes=0-10x", 100);
	ExpectIgnored("bytes=x0-10", 100);
	ExpectIgnored("bytes= 0-10", 100);
	ExpectIgnored("bytes=0-1-2", 100);
}

/**
 * strtoull() accepts a leading minus sign and silently negates the
 * value; none of the fields of a range-spec may be signed.
 */
TEST(HttpRange, NegativeNumbers)
{
	ExpectIgnored("bytes=0--1", 100);
	ExpectIgnored("bytes=-1-10", 100);
	ExpectIgnored("bytes=--1", 100);
	ExpectIgnored("bytes=-5--5", 100);
	ExpectIgnored("bytes=0--18446744073709551615", 100);
}

/**
 * A last-pos or suffix-length which does not fit into 64 bits is
 * necessarily past the end of the entity; and last-pos=UINT64_MAX
 * must not overflow the end offset calculation.
 */
TEST(HttpRange, Overflow)
{
	ExpectRange("bytes=0-18446744073709551615", 100, 0, 100);
	ExpectRange("bytes=0-18446744073709551616", 100, 0, 100);
	ExpectRange("bytes=0-99999999999999999999999", 100, 0, 100);
	ExpectRange("bytes=50-99999999999999999999999", 100, 50, 100);

	ExpectRange("bytes=-18446744073709551615", 100, 0, 100);
	ExpectRange("bytes=-99999999999999999999999", 100, 0, 100);

	ExpectUnsatisfiable("bytes=18446744073709551615-", 100);
	ExpectUnsatisfiable("bytes=99999999999999999999999-", 100);
	ExpectUnsatisfiable("bytes=99999999999999999999999-99999999999999999999999", 100);
}

/**
 * No range can be satisfied if the entity is empty.
 */
TEST(HttpRange, EmptyEntity)
{
	ExpectUnsatisfiable("bytes=0-", 0);
	ExpectUnsatisfiable("bytes=0-0", 0);
	ExpectUnsatisfiable("bytes=-1", 0);
	ExpectUnsatisfiable("bytes=-0", 0);
}
