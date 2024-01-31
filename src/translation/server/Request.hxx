// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "http/Status.hxx"

#include <cstdint>
#include <span>

enum class TranslationCommand : uint16_t;

namespace Translation::Server {

struct Request {
	unsigned protocol_version = 0;

	HttpStatus status = HttpStatus{};

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

	std::span<const std::byte> mount_listen_stream{};

	bool path_exists = false;

	bool login = false;

	bool cron = false;
};

} // namespace Translation::Server
