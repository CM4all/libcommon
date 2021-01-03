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

#include "ConfigParser.hxx"
#include "Config.hxx"
#include "io/FileLineParser.hxx"
#include "util/RuntimeError.hxx"
#include "util/CharUtil.hxx"
#include "util/StringAPI.hxx"
#include "util/StringStrip.hxx"

#include <cassert>
#include <climits>

#include <pwd.h>
#include <grp.h>
#include <inttypes.h>

static uid_t
ParseUser(const char *name)
{
	char *endptr;
	unsigned long i = strtoul(name, &endptr, 10);
	if (endptr > name && *endptr == 0)
		return i;

	const auto *pw = getpwnam(name);
	if (pw == nullptr)
		throw FormatRuntimeError("No such user: %s", name);

	return pw->pw_uid;
}

static gid_t
ParseGroup(const char *name)
{
	char *endptr;
	unsigned long i = strtoul(name, &endptr, 10);
	if (endptr > name && *endptr == 0)
		return i;

	const auto *gr = getgrnam(name);
	if (gr == nullptr)
		throw FormatRuntimeError("No such group: %s", name);

	return gr->gr_gid;
}

#ifdef HAVE_LIBSYSTEMD

static uint64_t
ParseUint64(const char *s)
{
	char *endptr;
	const auto value = strtoull(s, &endptr, 10);
	if (endptr == s || *endptr != 0)
		throw FormatRuntimeError("Failed to parse number: %s", s);

	return value;
}

static uint64_t
ParseRangeUint64(const char *s, uint64_t min, uint64_t max)
{
	const auto value = ParseUint64(s);
	if (value < min)
		throw FormatRuntimeError("Value too small; must be at least %" PRIu64, min);

	if (value > max)
		throw FormatRuntimeError("Value too large; must be at most %" PRIu64, min);

	return value;
}

static uint64_t
ParseCPUWeight(const char *s)
{
	return ParseRangeUint64(s, 1, 10000);
}

static uint64_t
ParseTasksMax(const char *s)
{
	return ParseRangeUint64(s, 1, uint64_t(1) << 31);
}

static uint64_t
ParseIOWeight(const char *s)
{
	return ParseRangeUint64(s, 1, 10000);
}

gcc_pure
static uint64_t
ParseByteUnit(const char *s) noexcept
{
	uint64_t value;

	switch (*s) {
	case 'B':
		++s;
		if (*s != 0)
			return 0;

		value = 1;
		break;

	case 'k':
	case 'K':
		value = uint64_t(1) << 10;
		break;

	case 'M':
		value = uint64_t(1) << 20;
		break;

	case 'G':
		value = uint64_t(1) << 30;
		break;

	case 'T':
		value = uint64_t(1) << 40;
		break;

	default:
		return 0;
	}

	++s;
	if (*s == 'i')
		++s;

	if (*s == 'B')
		++s;

	if (*s != 0)
		return 0;

	return value;
}

static uint64_t
ParsePositiveBytes(const char *s)
{
	char *endptr;
	uint64_t value = strtoull(s, &endptr, 10);
	if (endptr == s)
		throw FormatRuntimeError("Failed to parse number: %s", s);

	if (value == 0)
		throw std::runtime_error("Value must not be zero");

	s = StripLeft(endptr);
	if (*s != 0) {
		auto unit = ParseByteUnit(s);
		if (unit == 0)
			throw FormatRuntimeError("Unknown byte unit: %s", s);
		value *= unit;
	}

	return value;
}

static uint64_t
ParseMemorySize(const char *s)
{
	return ParsePositiveBytes(s);
}

#endif

void
SpawnConfigParser::ParseLine(FileLineParser &line)
{
	const char *word = line.ExpectWord();

	if (StringIsEqual(word, "allow_user")) {
		const char *s = line.ExpectValueAndEnd();

		if (IsDigitASCII(*s) && s[strlen(s) - 1] == '-') {
			char *endptr;
			unsigned long value = strtoul(s, &endptr, 0);
			assert(endptr > s);

			if (*endptr == '-' && endptr[1] == 0 && value > 0 &&
			    value <= UINT_MAX) {
				if (config.allow_all_uids_from <= 0 ||
				    value < config.allow_all_uids_from)
					config.allow_all_uids_from = value;
				return;
			}
		}

		config.allowed_uids.insert(ParseUser(s));
	} else if (StringIsEqual(word, "allow_group")) {
		config.allowed_gids.insert(ParseGroup(line.ExpectValueAndEnd()));
	} else if (StringIsEqual(word, "default_user")) {
		const char *s = line.ExpectValueAndEnd();

		if (!config.default_uid_gid.IsEmpty())
			throw std::runtime_error("Duplicate 'default_user'");

		config.default_uid_gid.Lookup(s);
#ifdef HAVE_LIBSYSTEMD
	} else if (StringIsEqualIgnoreCase(word, "CPUWeight")) {
		config.systemd_scope_properties.cpu_weight =
			ParseCPUWeight(line.ExpectValueAndEnd());
	} else if (StringIsEqualIgnoreCase(word, "TasksMax")) {
		config.systemd_scope_properties.tasks_max =
			ParseTasksMax(line.ExpectValueAndEnd());
	} else if (StringIsEqualIgnoreCase(word, "MemoryMin")) {
		config.systemd_scope_properties.memory_min =
			ParseMemorySize(line.ExpectValueAndEnd());
	} else if (StringIsEqualIgnoreCase(word, "MemoryLow")) {
		config.systemd_scope_properties.memory_low =
			ParseMemorySize(line.ExpectValueAndEnd());
	} else if (StringIsEqualIgnoreCase(word, "MemoryHigh")) {
		config.systemd_scope_properties.memory_high =
			ParseMemorySize(line.ExpectValueAndEnd());
	} else if (StringIsEqualIgnoreCase(word, "MemoryMax")) {
		config.systemd_scope_properties.memory_max =
			ParseMemorySize(line.ExpectValueAndEnd());
	} else if (StringIsEqualIgnoreCase(word, "MemorySwapMax")) {
		config.systemd_scope_properties.memory_swap_max =
			ParseMemorySize(line.ExpectValueAndEnd());
	} else if (StringIsEqualIgnoreCase(word, "IOWeight")) {
		config.systemd_scope_properties.io_weight =
			ParseIOWeight(line.ExpectValueAndEnd());
#endif
	} else
		throw LineParser::Error("Unknown option");
}
