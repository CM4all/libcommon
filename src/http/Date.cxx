// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Date.hxx"
#include "time/gmtime.hxx"
#include "util/CharUtil.hxx"
#include "util/DecimalFormat.hxx"

#include <array>
#include <cstdint>
#include <span>

#include <string.h>

using std::string_view_literals::operator""sv;

static constexpr std::array<char, 4>
operator ""_a4(const char *data, std::size_t size)
{
	if (size != 4)
		throw "Bad size";

	return {data[0], data[1], data[2], data[3]};
}

static constexpr std::array wdays = {
	"Sun,"_a4,
	"Mon,"_a4,
	"Tue,"_a4,
	"Wed,"_a4,
	"Thu,"_a4,
	"Fri,"_a4,
	"Sat,"_a4,
	"???,"_a4,
};

static constexpr std::array months = {
	"Jan "_a4,
	"Feb "_a4,
	"Mar "_a4,
	"Apr "_a4,
	"May "_a4,
	"Jun "_a4,
	"Jul "_a4,
	"Aug "_a4,
	"Sep "_a4,
	"Oct "_a4,
	"Nov "_a4,
	"Dec "_a4,
	"???,"_a4,
};

static constexpr std::span<const char, 4>
wday_name(int wday) noexcept
{
	return wdays[wday >= 0 && wday < 7
		     ? wday : 7];
}

static constexpr std::span<const char, 4>
month_name(int month) noexcept
{
	return months[month >= 0 && month < 12
		      ? month : 12];
}

static constexpr char *
Copy(char *dest, std::string_view src) noexcept
{
	return std::copy(src.begin(), src.end(), dest);
}

static constexpr char *
Copy(char *dest, std::span<const char> src) noexcept
{
	return std::copy(src.begin(), src.end(), dest);
}

char *
http_date_format_r(char *buffer,
		   std::chrono::system_clock::time_point t) noexcept
{
	const struct tm tm = sysx_time_gmtime(std::chrono::system_clock::to_time_t(t));

	buffer = Copy(buffer, wday_name(tm.tm_wday));
	*buffer++ = ' ';
	buffer = format_2digit(buffer, tm.tm_mday);
	*buffer++ = ' ';
	buffer = Copy(buffer, month_name(tm.tm_mon));
	buffer = format_4digit(buffer, tm.tm_year + 1900);
	*buffer++ = ' ';
	buffer = format_2digit(buffer, tm.tm_hour);
	*buffer++ = ':';
	buffer = format_2digit(buffer, tm.tm_min);
	*buffer++ = ':';
	buffer = format_2digit(buffer, tm.tm_sec);
	buffer = Copy(buffer, " GMT"sv);

	return buffer;
}

static char buffer[30];

const char *
http_date_format(std::chrono::system_clock::time_point t) noexcept
{
	*http_date_format_r(buffer, t) = '\0';
	return buffer;
}

static constexpr int
parse_2digit(const char *p) noexcept
{
	if (!IsDigitASCII(p[0]) || !IsDigitASCII(p[1]))
		return -1;

	return (p[0] - '0') * 10 + (p[1] - '0');
}

static constexpr int
parse_4digit(const char *p) noexcept
{
	if (!IsDigitASCII(p[0]) || !IsDigitASCII(p[1]) ||
	    !IsDigitASCII(p[2]) || !IsDigitASCII(p[3]))
		return -1;

	return (p[0] - '0') * 1000 + (p[1] - '0') * 100
		+ (p[2] - '0') * 10 + (p[3] - '0');
}

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif

static int
parse_month_name(const char *p) noexcept
{
	int i;

	for (i = 0; i < 12; ++i)
		if (*(const uint32_t *)(const void *)months[i].data() == *(const uint32_t *)(const void *)p)
			return i;

	return -1;
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

std::chrono::system_clock::time_point
http_date_parse(const char *p) noexcept
{
	struct tm tm;

	if (strlen(p) < 25)
		return std::chrono::system_clock::from_time_t(-1);

	tm.tm_sec = parse_2digit(p + 23);
	tm.tm_min = parse_2digit(p + 20);
	tm.tm_hour = parse_2digit(p + 17);
	tm.tm_mday = parse_2digit(p + 5);
	tm.tm_mon = parse_month_name(p + 8);
	tm.tm_year = parse_4digit(p + 12);

	if (tm.tm_sec == -1 || tm.tm_min == -1 || tm.tm_hour == -1 ||
	    tm.tm_mday == -1 || tm.tm_mon == -1 || tm.tm_year < 1900)
		return std::chrono::system_clock::from_time_t(-1);

	tm.tm_year -= 1900;
	tm.tm_isdst = -1;

	return std::chrono::system_clock::from_time_t(timegm(&tm));
}
