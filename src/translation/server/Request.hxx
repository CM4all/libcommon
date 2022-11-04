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

#pragma once

#include "http/Status.h"

#include <cstdint>
#include <span>

enum class TranslationCommand : uint16_t;

namespace Translation::Server {

struct Request {
	unsigned protocol_version = 0;

	http_status_t status = http_status_t(0);

	const char *uri = nullptr;

	const char *host = nullptr;

	std::span<const std::byte> session{};
	std::span<const std::byte> realm_session{};

	const char *param = nullptr;

	const char *user = nullptr;
	const char *password = nullptr;

	const char *args = nullptr, *query_string = nullptr;

	const char *widget_type = nullptr;

	const char *user_agent = nullptr;
	const char *accept_language = nullptr;

	/**
	 * The value of the "Authorization" HTTP request header.
	 */
	const char *authorization = nullptr;

	std::span<const std::byte> layout{};

	/**
	 * The matching BASE/REGEX response packet for the given
	 * LAYOUT.
	 */
	const char *base = nullptr, *regex = nullptr;

	std::span<const std::byte> error_document{};

	std::span<const std::byte> http_auth{};

	std::span<const std::byte> token_auth{};
	const char *auth_token = nullptr;

	const char *recover_session = nullptr;

	std::span<const std::byte> check{};

	const char *check_header = nullptr;

	std::span<const TranslationCommand> want{};
	std::span<const std::byte> want_full_uri{};

	std::span<const std::byte> chain{};

	const char *chain_header = nullptr;

	std::span<const std::byte> file_not_found{};

	std::span<const std::byte> content_type_lookup{};

	const char *suffix = nullptr;

	std::span<const std::byte> directory_index{};

	std::span<const std::byte> enotdir{};

	std::span<const std::byte> auth{};

	std::span<const std::byte> probe_path_suffixes{};
	const char *probe_suffix = nullptr;

	const char *listener_tag = nullptr;

	std::span<const std::byte> read_file{};
	std::span<const std::byte> internal_redirect{};

	const char *pool = nullptr;

	const char *execute = nullptr;

	const char *service = nullptr;

	const char *plan = nullptr;

	bool path_exists = false;

	bool login = false;

	bool cron = false;
};

} // namespace Translation::Server
