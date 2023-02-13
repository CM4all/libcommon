// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Headers.hxx"

#include <cstdint>
#include <string>

enum class HttpStatus : uint_least16_t;

struct StringCurlResponse {
	HttpStatus status;
	Curl::Headers headers;
	std::string body;
};
