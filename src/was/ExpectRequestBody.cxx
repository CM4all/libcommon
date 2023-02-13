// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "ExpectRequestBody.hxx"
#include "ExceptionResponse.hxx"
#include "util/MimeType.hxx"

#include <was/simple.h>

struct was_simple;

namespace Was {

[[gnu::pure]]
static bool
IsContentType(was_simple *w, const std::string_view expected) noexcept
{
	const char *content_type = was_simple_get_header(w, "content-type");
	return content_type != nullptr &&
		GetMimeTypeBase(content_type).compare(expected) == 0;
}

void
ExpectRequestBody(was_simple *w, const std::string_view content_type)
{
	if (!was_simple_has_body(w))
		throw BadRequest{"No request body\n"};

	if (!IsContentType(w, content_type))
		throw BadRequest{"Wrong request body type\n"};
}

} // namespace Was
