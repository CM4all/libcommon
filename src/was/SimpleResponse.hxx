// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <was/simple.h>

#include <string_view>

struct was_simple;

namespace Was {

bool
WriteResponseBody(struct was_simple *w, std::string_view body) noexcept;

bool
SendTextResponse(struct was_simple *w,
		 http_status_t status, std::string_view body) noexcept;

bool
SendNotFound(struct was_simple *w,
	     std::string_view body="Not Found\n") noexcept;

bool
SendBadRequest(struct was_simple *w,
	       std::string_view body="Bad Request\n") noexcept;

bool
SendMethodNotAllowed(struct was_simple *w, const char *allow) noexcept;

bool
SendConflict(struct was_simple *w,
	     std::string_view body="Conflict\n") noexcept;

} // namespace Was
