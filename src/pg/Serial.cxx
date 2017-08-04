/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Serial.hxx"

#include <stdexcept>

#include <stdlib.h>

namespace Pg {

Serial
Serial::Parse(const char *s)
{
	char *endptr;
	auto value = strtol(s, &endptr, 10);
	if (endptr == s || *endptr != 0)
		throw std::invalid_argument("Failed to parse serial");

	return Serial(value);
}

}
