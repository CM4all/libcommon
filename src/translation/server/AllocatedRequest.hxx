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

#include "Request.hxx"
#include "util/AllocatedArray.hxx"

#include <string>

#include <stdint.h>

enum class TranslationCommand : uint16_t;
template<typename T> struct ConstBuffer;

namespace Translation::Server {

class AllocatedRequest : public Request {
	std::string uri_buffer, host_buffer;
	std::string session_buffer;
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
	std::string service_buffer;

public:
	/**
	 * Throws on error.
	 */
	void Parse(TranslationCommand cmd, ConstBuffer<void> payload);
};

} // namespace Translation::Server
