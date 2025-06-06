// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "Request.hxx"
#include "util/AllocatedArray.hxx"

#include <string>

#include <stdint.h>

enum class TranslationCommand : uint16_t;

namespace Translation::Server {

class AllocatedRequest : public Request {
	std::string uri_buffer, host_buffer;
	std::string session_buffer;
	std::string realm_session_buffer;
	std::string param_buffer;
	std::string user_buffer;
	std::string password_buffer;
	std::string args_buffer, query_string_buffer;

	std::string widget_type_buffer;

	std::string user_agent_buffer;
	std::string accept_language_buffer;
	std::string authorization_buffer;
	std::string layout_buffer;
	std::string base_buffer, regex_buffer;
	std::string error_document_buffer;
	std::string http_auth_buffer;
	std::string token_auth_buffer;
	std::string auth_token_buffer;
	std::string recover_session_buffer;
	std::string check_buffer;
	std::string check_header_buffer;
	AllocatedArray<TranslationCommand> want_buffer;
	std::string want_full_uri_buffer;
	std::string chain_buffer, chain_header_buffer;
	std::string file_not_found_buffer, content_type_lookup_buffer;
	std::string suffix_buffer;
	std::string directory_index_buffer, enotdir_buffer;
	std::string auth_buffer;

	std::string probe_path_suffixes_buffer, probe_suffix_buffer;

	std::string listener_tag_buffer;
	std::string read_file_buffer;
	std::string internal_redirect_buffer;
	std::string pool_buffer;
	std::string execute_buffer;
	std::string service_buffer;
	std::string plan_buffer;
	std::string mount_listen_stream_buffer;

public:
	/**
	 * Throws on error.
	 */
	void Parse(TranslationCommand cmd, std::span<const std::byte> payload);
};

} // namespace Translation::Server
