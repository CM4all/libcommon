/*
 * Copyright 2007-2021 CM4all GmbH
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

#ifndef BENG_PROXY_TRANSLATE_RESPONSE_HXX
#define BENG_PROXY_TRANSLATE_RESPONSE_HXX

#include "translation/Features.hxx"
#include "util/ConstBuffer.hxx"
#if TRANSLATION_ENABLE_HTTP
#include "util/kvlist.hxx"
#include "bp/ForwardHeaders.hxx"
#endif
#if TRANSLATION_ENABLE_WIDGET
#include "util/StringSet.hxx"
#endif
#if TRANSLATION_ENABLE_RADDRESS
#include "ResourceAddress.hxx"
#endif
#if TRANSLATION_ENABLE_EXECUTE
#include "adata/ExpandableStringList.hxx"
#include "spawn/ChildOptions.hxx"
#endif

#if TRANSLATION_ENABLE_HTTP
#include "http/Status.h"
#endif

#include <chrono>

#include <assert.h>
#include <stdint.h>

enum class TranslationCommand : uint16_t;
struct WidgetView;
struct TranslationLayoutItem;
class AllocatorPtr;
class UniqueRegex;
class MatchData;

struct TranslateResponse {
	/**
	 * The protocol version from the BEGIN packet.
	 */
	unsigned protocol_version;

	std::chrono::duration<uint32_t> max_age;

	/**
	 * From #TranslationCommand::EXPIRES_RELATIVE
	 */
	std::chrono::duration<uint32_t> expires_relative;

#if TRANSLATION_ENABLE_HTTP
	http_status_t status;
#else
	int status;
#endif

	const char *token;

#if TRANSLATION_ENABLE_EXECUTE
	const char *shell;

	const char *execute;

	/**
	 * Command-line arguments for #execute.
	 */
	ExpandableStringList args;

	ChildOptions child_options;
#endif

#if TRANSLATION_ENABLE_RADDRESS
	ResourceAddress address;

	const char *base;

	ConstBuffer<void> layout;
	ConstBuffer<TranslationLayoutItem> layout_items;
#endif

#if TRANSLATION_ENABLE_EXPAND
	const char *regex;
	const char *inverse_regex;
#endif

	const char *site;

	const char *like_host;
	const char *canonical_host;

#if TRANSLATION_ENABLE_HTTP
	const char *document_root;

	const char *redirect;
	const char *bounce;
#endif

	const char *message;

#if TRANSLATION_ENABLE_HTTP
	const char *scheme;
	const char *host;
	const char *uri;

	const char *local_uri;

	const char *untrusted;
	const char *untrusted_prefix;
	const char *untrusted_site_suffix;
	const char *untrusted_raw_site_suffix;
#endif

	/**
	 * @see #TranslationCommand::TEST_PATH
	 */
	const char *test_path;

	const char *stats_tag;

#if TRANSLATION_ENABLE_SESSION
	ConstBuffer<void> session;
	ConstBuffer<void> attach_session;
#endif

	const char *pool;

#if TRANSLATION_ENABLE_HTTP
	/**
	 * The payload of the #TranslationCommand::INTERNAL_REDIRECT
	 * packet.  If ConstBuffer::IsNull(), then no
	 * #TranslationCommand::INTERNAL_REDIRECT packet was received.
	 */
	ConstBuffer<void> internal_redirect;

	/**
	 * The payload of the HTTP_AUTH packet.  If
	 * ConstBuffer::IsNull(), then no HTTP_AUTH packet was
	 * received.
	 */
	ConstBuffer<void> http_auth;

	/**
	 * The payload of the TOKEN_AUTH packet.  If
	 * ConstBuffer::IsNull(), then no TOKEN_AUTH packet was
	 * received.
	 */
	ConstBuffer<void> token_auth;
#endif

#if TRANSLATION_ENABLE_SESSION
	/**
	 * The payload of the CHECK packet.  If ConstBuffer::IsNull(),
	 * then no CHECK packet was received.
	 */
	ConstBuffer<void> check;

	/**
	 * The payload of the AUTH packet.  If ConstBuffer::IsNull(), then
	 * no AUTH packet was received.
	 */
	ConstBuffer<void> auth;

	/**
	 * @see #TranslationCommand::AUTH_FILE,
	 * #TranslationCommand::EXPAND_AUTH_FILE
	 */
	const char *auth_file;

	/**
	 * @see #TranslationCommand::APPEND_AUTH
	 */
	ConstBuffer<void> append_auth;

	/**
	 * @see #TranslationCommand::EXPAND_APPEND_AUTH
	 */
	const char *expand_append_auth;
#endif

#if TRANSLATION_ENABLE_HTTP
	/**
	 * The payload of the #TranslationCommand::WANT_FULL_URI packet.
	 * If ConstBuffer::IsNull(), then no
	 * #TranslationCommand::WANT_FULL_URI packet was received.
	 */
	ConstBuffer<void> want_full_uri;

	/**
	 * The payload of the #TranslationCommand::CHAIN
	 * packet.
	 */
	ConstBuffer<void> chain;
#endif

#if TRANSLATION_ENABLE_SESSION
	const char *user;

	const char *session_site;

	const char *language;

	const char *realm;

	HttpAddress *external_session_manager;

	/**
	 * The value of the "WWW-Authenticate" HTTP response header.
	 */
	const char *www_authenticate;

	/**
	 * The value of the "Authentication-Info" HTTP response header.
	 */
	const char *authentication_info;

	const char *cookie_domain;
	const char *cookie_host;
	const char *cookie_path;

	const char *recover_session;
#endif

#if TRANSLATION_ENABLE_HTTP
	KeyValueList request_headers;
	KeyValueList expand_request_headers;

	KeyValueList response_headers;
	KeyValueList expand_response_headers;
#endif

#if TRANSLATION_ENABLE_WIDGET
	WidgetView *views;

	/**
	 * From #TranslationCommand::WIDGET_GROUP.
	 */
	const char *widget_group;

	/**
	 * From #TranslationCommand::GROUP_CONTAINER.
	 */
	StringSet container_groups;
#endif

#if TRANSLATION_ENABLE_CACHE
	/**
	 * From #TranslationCommand::CACHE_TAG.
	 */
	const char *cache_tag;

	ConstBuffer<TranslationCommand> vary;
	ConstBuffer<TranslationCommand> invalidate;
#endif

#if TRANSLATION_ENABLE_WANT
	ConstBuffer<TranslationCommand> want;
#endif

#if TRANSLATION_ENABLE_RADDRESS
	ConstBuffer<void> file_not_found;

	/**
	 * From #TranslationCommand::CONTENT_TYPE, but only in reply to
	 * #TranslationCommand::CONTENT_TYPE_LOOKUP /
	 * #TranslationCommand::SUFFIX.
	 */
	const char *content_type;

	ConstBuffer<void> enotdir;

	ConstBuffer<void> directory_index;
#endif

	ConstBuffer<void> error_document;

	/**
	 * From #TranslationCommand::PROBE_PATH_SUFFIXES.
	 */
	ConstBuffer<void> probe_path_suffixes;

	ConstBuffer<const char *> probe_suffixes;

	const char *read_file;

	struct {
		uint64_t mtime;
		const char *path;
	} validate_mtime;

#if TRANSLATION_ENABLE_SESSION
	std::chrono::duration<uint32_t> user_max_age;
	std::chrono::duration<uint16_t> external_session_keepalive;
#endif

#if TRANSLATION_ENABLE_HTTP
	/**
	 * Non-zero means the TranslationCommand::HTTPS_ONLY packet was
	 * received, and the value is the port number.
	 */
	uint16_t https_only;

	/**
	 * Which request headers are forwarded?
	 */
	HeaderForwardSettings request_header_forward;

	/**
	 * Which response headers are forwarded?
	 */
	HeaderForwardSettings response_header_forward;
#endif

#if TRANSLATION_ENABLE_CACHE
	bool uncached;
#endif

#if TRANSLATION_ENABLE_RADDRESS
	bool unsafe_base;

	bool easy_base;
#endif

#if TRANSLATION_ENABLE_EXPAND
	bool regex_tail, regex_unescape, inverse_regex_unescape;

	bool expand_site;
	bool expand_document_root;
	bool expand_redirect;
	bool expand_uri;
	bool expand_test_path;
	bool expand_auth_file;
	bool expand_cookie_host;
	bool expand_read_file;
#endif

#if TRANSLATION_ENABLE_WIDGET
	bool direct_addressing;
#endif

#if TRANSLATION_ENABLE_SESSION
	bool stateful;

	bool discard_session, discard_realm_session;

	bool secure_cookie;

	bool require_csrf_token, send_csrf_token;

	/**
	 * @see #TranslationCommand::REALM_FROM_AUTH_BASE
	 */
	bool realm_from_auth_base;
#endif

#if TRANSLATION_ENABLE_TRANSFORMATION
	bool filter_4xx;

	bool subst_alt_syntax;
#endif

	bool defer;

	bool previous;

	bool transparent;

#if TRANSLATION_ENABLE_HTTP
	bool redirect_query_string;
	bool redirect_full_uri;
#endif

#if TRANSLATION_ENABLE_RADDRESS
	bool auto_base;
#endif

#if TRANSLATION_ENABLE_WIDGET
	bool widget_info;

	bool anchor_absolute;
#endif

#if TRANSLATION_ENABLE_HTTP
	bool tiny_image;

	bool transparent_chain;
	bool break_chain;

	bool dump_headers;
#endif

#if TRANSLATION_ENABLE_EXPAND
	/**
	 * @see #TranslationCommand::REGEX_ON_HOST_URI
	 */
	bool regex_on_host_uri;

	/**
	 * @see #TranslationCommand::REGEX_ON_USER_URI
	 */
	bool regex_on_user_uri;
#endif

	/**
	 * @see #TranslationCommand::AUTO_DEFLATE
	 */
	bool auto_deflate;

	/**
	 * @see #TranslationCommand::AUTO_GZIP
	 */
	bool auto_gzip;

	TranslateResponse() = default;
	TranslateResponse(TranslateResponse &&) = default;
	TranslateResponse &operator=(TranslateResponse &&) = default;

	void Clear();

#if TRANSLATION_ENABLE_WANT
	bool Wants(TranslationCommand cmd) const {
		assert(protocol_version >= 1);

		return want.Contains(cmd);
	}
#endif

#if TRANSLATION_ENABLE_CACHE
	[[gnu::pure]]
	bool VaryContains(TranslationCommand cmd) const {
		return vary.Contains(cmd);
	}
#endif

#if TRANSLATION_ENABLE_SESSION
	[[gnu::pure]]
	bool HasAuth() const {
		return !auth.IsNull() ||
			auth_file != nullptr;
	}

	bool HasUntrusted() const {
		return untrusted != nullptr || untrusted_prefix != nullptr ||
			untrusted_site_suffix != nullptr ||
			untrusted_raw_site_suffix != nullptr;
	}
#endif

	void CopyFrom(AllocatorPtr alloc, const TranslateResponse &src);

#if TRANSLATION_ENABLE_CACHE
	/**
	 * Copy data from #src for storing in the translation cache.
	 *
	 * Throws HttpMessageResponse(HTTP_STATUS_BAD_GATEWAY) on base
	 * mismatch.
	 */
	void CacheStore(AllocatorPtr alloc, const TranslateResponse &src,
			const char *uri);

	/**
	 * Throws std::runtime_error on error.
	 */
	void CacheLoad(AllocatorPtr alloc, const TranslateResponse &src,
		       const char *uri);
#endif

	/**
	 * Throws std::runtime_error on error.
	 */
	UniqueRegex CompileRegex() const;
	UniqueRegex CompileInverseRegex() const;

#if TRANSLATION_ENABLE_EXPAND
	/**
	 * Does any response need to be expanded with
	 * translate_response_expand()?
	 */
	[[gnu::pure]]
	bool IsExpandable() const;

	/**
	 * Expand the strings in this response with the specified regex
	 * result.
	 *
	 * Throws std::runtime_error on error.
	 */
	void Expand(AllocatorPtr alloc, const MatchData &match_data);
#endif
};

#endif
