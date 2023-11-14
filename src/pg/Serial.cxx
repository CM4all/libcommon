// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Serial.hxx"
#include "util/NumberParser.hxx"

#include <stdexcept>

namespace Pg {

Serial
Serial::Parse(std::string_view s)
{
	const auto value = ParseInteger<value_type>(s);
	if (!value)
		throw std::invalid_argument("Failed to parse serial");

	return Serial{*value};
}

BigSerial
BigSerial::Parse(std::string_view s)
{
	const auto value = ParseInteger<value_type>(s);
	if (!value)
		throw std::invalid_argument("Failed to parse serial");

	return BigSerial{*value};
}

}
