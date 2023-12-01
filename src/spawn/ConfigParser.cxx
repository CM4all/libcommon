// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "ConfigParser.hxx"
#include "Config.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "io/config/FileLineParser.hxx"
#include "util/CharUtil.hxx"
#include "util/StringAPI.hxx"
#include "util/StringStrip.hxx"

#include <cassert>
#include <climits>

#include <pwd.h>
#include <grp.h>
#include <unistd.h> // for sysconf()

static uid_t
ParseUser(const char *name)
{
	char *endptr;
	unsigned long i = strtoul(name, &endptr, 10);
	if (endptr > name && *endptr == 0)
		return i;

	const auto *pw = getpwnam(name);
	if (pw == nullptr)
		throw FmtRuntimeError("No such user: {}", name);

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
		throw FmtRuntimeError("No such group: {}", name);

	return gr->gr_gid;
}

#ifdef HAVE_LIBSYSTEMD

static uint64_t
ParseUint64(const char *s)
{
	char *endptr;
	const auto value = strtoull(s, &endptr, 10);
	if (endptr == s || *endptr != 0)
		throw FmtRuntimeError("Failed to parse number: '{}'", s);

	return value;
}

static uint64_t
ParseRangeUint64(const char *s, uint64_t min, uint64_t max)
{
	const auto value = ParseUint64(s);
	if (value < min)
		throw FmtRuntimeError("Value too small; must be at least {}", min);

	if (value > max)
		throw FmtRuntimeError("Value too large; must be at most {}", min);

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

[[gnu::pure]]
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
		throw FmtRuntimeError("Failed to parse number: '{}'", s);

	if (value == 0)
		throw std::runtime_error("Value must not be zero");

	s = StripLeft(endptr);
	if (*s != 0) {
		auto unit = ParseByteUnit(s);
		if (unit == 0)
			throw FmtRuntimeError("Unknown byte unit: '{}'", s);
		value *= unit;
	}

	return value;
}

static uint64_t
ParseMemorySize(const char *s)
{
	return ParsePositiveBytes(s);
}

static uint64_t
ParseMemoryLimit(const char *s)
{
	if (StringIsEqual(s, "infinity"))
		return UINT64_MAX;

	return ParseMemorySize(s);
}

static uint64_t
ParsePhysicalMemoryLimit(const char *s)
{
	if (strchr(s, '%') != nullptr) {
		char *endptr;
		uint64_t value = strtoull(s, &endptr, 10);
		if (endptr == s || *endptr != '%' || endptr[1] != 0)
			throw FmtRuntimeError("Failed to parse percent number: '{}'", s);

		if (value == 0)
			throw std::runtime_error("Value must not be zero");

		const uint64_t page_size = sysconf(_SC_PAGESIZE);
		const uint64_t phys_pages = sysconf(_SC_PHYS_PAGES);

		const uint64_t n_pages = (value * phys_pages) / 100;
		return n_pages * page_size;
	}

	return ParseMemoryLimit(s);
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
	} else if (StringIsEqual(word, "cgroups_writable_by_group")) {
		config.cgroups_writable_by_gid = ParseGroup(line.ExpectValueAndEnd());
	} else if (StringIsEqual(word, "default_user")) {
		const char *s = line.ExpectValueAndEnd();

		if (!config.default_uid_gid.IsEmpty())
			throw std::runtime_error("Duplicate 'default_user'");

		config.default_uid_gid.Lookup(s);
	} else if (StringIsEqual(word, "systemd_scope_optional")) {
		config.systemd_scope_optional = line.NextBool();
		line.ExpectEnd();
#ifdef HAVE_LIBSYSTEMD
	} else if (StringIsEqualIgnoreCase(word, "CPUWeight")) {
		config.systemd_scope_properties.cpu_weight =
			ParseCPUWeight(line.ExpectValueAndEnd());
	} else if (StringIsEqualIgnoreCase(word, "TasksMax")) {
		config.systemd_scope_properties.tasks_max =
			ParseTasksMax(line.ExpectValueAndEnd());
	} else if (StringIsEqualIgnoreCase(word, "MemoryMin")) {
		config.systemd_scope_properties.memory_min =
			ParsePhysicalMemoryLimit(line.ExpectValueAndEnd());
	} else if (StringIsEqualIgnoreCase(word, "MemoryLow")) {
		config.systemd_scope_properties.memory_low =
			ParsePhysicalMemoryLimit(line.ExpectValueAndEnd());
	} else if (StringIsEqualIgnoreCase(word, "MemoryHigh")) {
		config.systemd_scope_properties.memory_high =
			ParsePhysicalMemoryLimit(line.ExpectValueAndEnd());
	} else if (StringIsEqualIgnoreCase(word, "MemoryMax")) {
		config.systemd_scope_properties.memory_max =
			ParsePhysicalMemoryLimit(line.ExpectValueAndEnd());
	} else if (StringIsEqualIgnoreCase(word, "MemorySwapMax")) {
		config.systemd_scope_properties.memory_swap_max =
			ParseMemoryLimit(line.ExpectValueAndEnd());
	} else if (StringIsEqualIgnoreCase(word, "IOWeight")) {
		config.systemd_scope_properties.io_weight =
			ParseIOWeight(line.ExpectValueAndEnd());
#endif
	} else
		throw LineParser::Error("Unknown option");
}
