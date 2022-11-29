/*
 * Copyright 2007-2022 CM4all GmbH
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

#include "String.hxx"
#include "Protocol.hxx"
#include "util/StringAPI.hxx"

#include <stdexcept>

namespace Net {
namespace Log {

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
	throw std::invalid_argument("Invalid log record type");
}

}}
