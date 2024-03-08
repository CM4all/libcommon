// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "String.hxx"
#include "Protocol.hxx"
#include "util/StringAPI.hxx"

#include <stdexcept>

namespace Net::Log {

const char *
ToString(Type type) noexcept
{
	switch (type) {
	case Type::UNSPECIFIED:
		return "unspecified";

	case Type::HTTP_ACCESS:
		return "http_access";

	case Type::HTTP_ERROR:
		return "http_error";

	case Type::SUBMISSION:
		return "submission";

	case Type::SSH:
		return "ssh";

	case Type::JOB:
		return "job";

	case Type::HISTORY:
		return "history";
	}

	return nullptr;
}

Type
ParseType(const char *s)
{
	if (StringIsEqual(s, "unspecified"))
		return Type::UNSPECIFIED;
	if (StringIsEqual(s, "http_access"))
		return Type::HTTP_ACCESS;
	if (StringIsEqual(s, "http_error"))
		return Type::HTTP_ERROR;
	if (StringIsEqual(s, "submission"))
		return Type::SUBMISSION;
	if (StringIsEqual(s, "ssh"))
		return Type::SSH;
	if (StringIsEqual(s, "job"))
		return Type::JOB;
	else if (StringIsEqual(s, "history"))
		return Type::HISTORY;
	throw std::invalid_argument("Invalid log record type");
}

} // namespace Net::Log
