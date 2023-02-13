// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Serial.hxx"

#include <stdexcept>
#include <cstdlib>

namespace Pg {

Serial
Serial::Parse(const char *s)
{
	char *endptr;
	auto value = std::strtoul(s, &endptr, 10);
	if (endptr == s || *endptr != 0)
		throw std::invalid_argument("Failed to parse serial");

	return Serial(value);
}

BigSerial
BigSerial::Parse(const char *s)
{
	char *endptr;
	auto value = std::strtoul(s, &endptr, 10);
	if (endptr == s || *endptr != 0)
		throw std::invalid_argument("Failed to parse serial");

	return BigSerial{value};
}

}
