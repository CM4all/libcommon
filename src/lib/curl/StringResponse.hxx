// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "Headers.hxx"

#include <cstdint>
#include <string>

enum class HttpStatus : uint_least16_t;

namespace Curl {

struct StringResponse {
	HttpStatus status;
	Curl::Headers headers;
	std::string body;
};

} // namespace Curl
