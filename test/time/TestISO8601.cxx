// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "time/ISO8601.hxx"

#include <gtest/gtest.h>

static constexpr struct {
	const char *s;
	time_t t;
	std::chrono::system_clock::duration d;
} parse_tests[] = {
	/* full ISO8601 */
	{ "1970-01-01T00:00:00Z", 0, std::chrono::seconds(1) },
	{ "1970-01-01T00:00:01Z", 1, std::chrono::seconds(1) },
	{ "2019-02-04T16:46:41Z", 1549298801, std::chrono::seconds(1) },
	{ "2018-12-31T23:59:59Z", 1546300799, std::chrono::seconds(1) },
	{ "2019-01-01T00:00:00Z", 1546300800, std::chrono::seconds(1) },

	/* full month */
	{ "1970-01", 0, std::chrono::hours(24 * 31) },
	{ "2019-02", 1548979200, std::chrono::hours(24 * 28) },
	{ "2019-01", 1546300800, std::chrono::hours(24 * 31) },

	/* only date */
	{ "1970-01-01", 0, std::chrono::hours(24) },
	{ "2019-02-04", 1549238400, std::chrono::hours(24) },
	{ "2018-12-31", 1546214400, std::chrono::hours(24) },
	{ "2019-01-01", 1546300800, std::chrono::hours(24) },

	/* date with time zone */
	{ "2019-02-04Z", 1549238400, std::chrono::hours(24) },

	/* without time zone */
	{ "2019-02-04T16:46:41", 1549298801, std::chrono::seconds(1) },

	/* without seconds */
	{ "2019-02-04T16:46", 1549298760, std::chrono::minutes(1) },
	{ "2019-02-04T16:46Z", 1549298760, std::chrono::minutes(1) },

	/* without minutes */
	{ "2019-02-04T16", 1549296000, std::chrono::hours(1) },
	{ "2019-02-04T16Z", 1549296000, std::chrono::hours(1) },

	/* with time zone */
	{ "2019-02-04T16:46:41+02", 1549291601, std::chrono::seconds(1) },
	{ "2019-02-04T16:46:41+0200", 1549291601, std::chrono::seconds(1) },
	{ "2019-02-04T16:46:41+02:00", 1549291601, std::chrono::seconds(1) },
	{ "2019-02-04T16:46:41-0200", 1549306001, std::chrono::seconds(1) },

	/* without field separators */
	{ "19700101T000000Z", 0, std::chrono::seconds(1) },
	{ "19700101T000001Z", 1, std::chrono::seconds(1) },
	{ "20190204T164641Z", 1549298801, std::chrono::seconds(1) },
	{ "19700101", 0, std::chrono::hours(24) },
	{ "20190204", 1549238400, std::chrono::hours(24) },
	{ "20190204T1646", 1549298760, std::chrono::minutes(1) },
	{ "20190204T16", 1549296000, std::chrono::hours(1) },
};

TEST(ISO8601, Parse)
{
#ifdef _WIN32
	// TODO: re-enable when ParseISO8601() has been implemented on Windows
	GTEST_SKIP();
#endif

	for (const auto &i : parse_tests) {
		const auto result = ParseISO8601(i.s);
		EXPECT_EQ(std::chrono::system_clock::to_time_t(result.first), i.t);
		EXPECT_EQ(result.second, i.d);
	}
}
