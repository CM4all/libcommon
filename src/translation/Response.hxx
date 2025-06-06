// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "translation/Features.hxx"
#if TRANSLATION_ENABLE_HTTP
#include "TokenBucketParams.hxx"
#include "net/NetworkList.hxx"
#include "util/kvlist.hxx"
#include "bp/ForwardHeaders.hxx"
#endif
#if TRANSLATION_ENABLE_WIDGET
#include "widget/VList.hxx"
#include "util/StringSet.hxx"
#endif
#if TRANSLATION_ENABLE_RADDRESS
#include "ResourceAddress.hxx"
#include <memory>
#include <vector>
#endif

#if TRANSLATION_ENABLE_SESSION
#include "http/CookieSameSite.hxx"
#endif

#if TRANSLATION_ENABLE_EXECUTE_SERVICE
#include "adata/SimpleStringMap.hxx"
#endif

#include <algorithm> // for std::find()
#include <chrono>
#include <span>

#include <assert.h>
#include <stdint.h>

enum class HttpStatus : uint_least16_t;
enum class TranslationCommand : uint16_t;
enum class Arch : uint_least8_t;
struct TranslationLayoutItem;
class AllocatorPtr;
class UniqueRegex;
class MatchData;
struct ExecuteOptions;
struct ServiceExecuteOptions;

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

	/**
	 * From #TranslationCommand::EXPIRES_RELATIVE_WITH_QUERY
	 */
	std::chrono::duration<uint32_t> expires_relative_with_query;

	HttpStatus status;

	const char *token;

	const char *analytics_id;

	const char *generator;

#if TRANSLATION_ENABLE_LOGIN
	/**
	 * @see #TranslationCommand::NO_PASSWORD
	 *
	 * If the payload is empty, then this is an empty string.
	 */
	const char *no_password;

	const char *authorized_keys;
#endif

#if TRANSLATION_ENABLE_EXECUTE
	ExecuteOptions *execute_options;

#if TRANSLATION_ENABLE_EXECUTE_SERVICE
	SimpleStringMap<ExecuteOptions> service_execute_options;
#endif
#endif

#if TRANSLATION_ENABLE_RADDRESS
	ResourceAddress address;

	const char *base;

	std::span<const std::byte> layout;
	std::shared_ptr<std::vector<TranslationLayoutItem>> layout_items;
#endif

#if TRANSLATION_ENABLE_EXPAND
	const char *regex;
	const char *inverse_regex;
#endif

	/**
	 * Override the LISTENER_TAG.
	 *
	 * @see TranslationCommand::LISTENER_TAG
	 */
	const char *listener_tag;

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
	std::span<const std::byte> session;
	std::span<const std::byte> realm_session;
	std::span<const std::byte> attach_session;
#endif

	const char *pool;

#if TRANSLATION_ENABLE_HTTP
	/**
	 * The payload of the #TranslationCommand::INTERNAL_REDIRECT
	 * packet.  If nullptr, then no
	 * #TranslationCommand::INTERNAL_REDIRECT packet was received.
	 */
	std::span<const std::byte> internal_redirect;

	/**
	 * The payload of the HTTP_AUTH packet.  If
	 * nullptr, then no HTTP_AUTH packet was
	 * received.
	 */
	std::span<const std::byte> http_auth;

	/**
	 * The payload of the TOKEN_AUTH packet.  If
	 * nullptr, then no TOKEN_AUTH packet was
	 * received.
	 */
	std::span<const std::byte> token_auth;
#endif

#if TRANSLATION_ENABLE_SESSION
	/**
	 * The payload of the CHECK packet.  If nullptr,
	 * then no CHECK packet was received.
	 */
	std::span<const std::byte> check;

	const char *check_header;

	/**
	 * The payload of the AUTH packet.  If nullptr, then
	 * no AUTH packet was received.
	 */
	std::span<const std::byte> auth;

	/**
	 * @see #TranslationCommand::AUTH_FILE,
	 * #TranslationCommand::EXPAND_AUTH_FILE
	 */
	const char *auth_file;

	/**
	 * @see #TranslationCommand::APPEND_AUTH
	 */
	std::span<const std::byte> append_auth;

	/**
	 * @see #TranslationCommand::EXPAND_APPEND_AUTH
	 */
	const char *expand_append_auth;
#endif

#if TRANSLATION_ENABLE_HTTP
	/**
	 * The payload of the #TranslationCommand::WANT_FULL_URI packet.
	 * If nullptr, then no
	 * #TranslationCommand::WANT_FULL_URI packet was received.
	 */
	std::span<const std::byte> want_full_uri;

	/**
	 * The payload of the #TranslationCommand::CHAIN
	 * packet.
	 */
	std::span<const std::byte> chain;
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

	NetworkList allow_remote_networks;
#endif

#if TRANSLATION_ENABLE_WIDGET
	WidgetViewList views;

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

	std::span<const TranslationCommand> vary;
	std::span<const TranslationCommand> invalidate;
#endif

#if TRANSLATION_ENABLE_WANT
	std::span<const TranslationCommand> want;
#endif

#if TRANSLATION_ENABLE_RADDRESS
	std::span<const std::byte> file_not_found;

	/**
	 * From #TranslationCommand::CONTENT_TYPE, but only in reply to
	 * #TranslationCommand::CONTENT_TYPE_LOOKUP /
	 * #TranslationCommand::SUFFIX.
	 */
	const char *content_type;

	std::span<const std::byte> enotdir;

	std::span<const std::byte> directory_index;
#endif

	std::span<const std::byte> error_document;

	/**
	 * From #TranslationCommand::PROBE_PATH_SUFFIXES.
	 */
	std::span<const std::byte> probe_path_suffixes;

	std::span<const char *const> probe_suffixes;

	const char *read_file;

	struct {
		uint64_t mtime;
		const char *path;
	} validate_mtime;

#if TRANSLATION_ENABLE_HTTP
	TranslateTokenBucketParams rate_limit_site_requests;
	TranslateTokenBucketParams rate_limit_site_traffic;
#endif

	/**
	 * From #TranslationCommand::TIMEOUT.  Zero if unset.
	 */
	std::chrono::duration<uint_least32_t> timeout;

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

	Arch arch;

#if TRANSLATION_ENABLE_CACHE
	bool uncached;

	bool ignore_no_cache;

	bool eager_cache;

	bool auto_flush_cache;
#endif

#if TRANSLATION_ENABLE_RADDRESS
	bool unsafe_base;

	bool easy_base;

	bool path_exists;
#endif

#if TRANSLATION_ENABLE_EXPAND
	bool regex_tail, regex_raw, regex_unescape, inverse_regex_unescape;

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
	CookieSameSite session_cookie_same_site;

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
#endif

	bool defer;

	bool previous;

	bool transparent;

#if TRANSLATION_ENABLE_HTTP
	bool discard_query_string;
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
	bool accept_http;
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

#if TRANSLATION_ENABLE_RADDRESS
	/**
	 * @see #TranslationCommand::AUTO_GZIP
	 */
	bool auto_gzip;

	/**
	 * @see #TranslationCommand::AUTO_BROTLI
	 */
	bool auto_brotli;

	/**
	 * @see #TranslationCommand::AUTO_COMPRESS_ONLY_TEXT
	 */
	bool auto_compress_only_text;

	/**
	 * @see #TranslationCommand::AUTO_GZIPPED
	 */
	bool auto_gzipped;

	/**
	 * @see #TranslationCommand::AUTO_BROTLI_PATH
	 */
	bool auto_brotli_path;
#endif

#if TRANSLATION_ENABLE_LOGIN
	bool no_home_authorized_keys;
#endif

	constexpr TranslateResponse() noexcept = default;
	TranslateResponse(TranslateResponse &&) = default;
	TranslateResponse &operator=(TranslateResponse &&) = default;

	void Clear() noexcept;

#if TRANSLATION_ENABLE_WANT
	[[nodiscard]]
	constexpr bool Wants(TranslationCommand cmd) const noexcept {
		assert(protocol_version >= 1);

		return std::find(want.begin(), want.end(), cmd) != want.end();
	}
#endif

#if TRANSLATION_ENABLE_CACHE
	[[nodiscard]] [[gnu::pure]]
	bool VaryContains(TranslationCommand cmd) const noexcept {
		return std::find(vary.begin(), vary.end(), cmd) != vary.end();
	}
#endif

#if TRANSLATION_ENABLE_SESSION
	[[nodiscard]] [[gnu::pure]]
	constexpr bool HasAuth() const noexcept {
		return auth.data() != nullptr ||
			auth_file != nullptr;
	}

	[[nodiscard]]
	constexpr bool HasUntrusted() const noexcept {
		return untrusted != nullptr || untrusted_prefix != nullptr ||
			untrusted_site_suffix != nullptr ||
			untrusted_raw_site_suffix != nullptr;
	}
#endif

#if TRANSLATION_ENABLE_RADDRESS
	constexpr bool HasAutoCompress() const noexcept {
		return auto_gzip || auto_brotli;
	}
#endif

	[[nodiscard]]
	constexpr auto GetExpiresRelative(bool has_query) const noexcept {
		return has_query && expires_relative_with_query.count() > 0
			? expires_relative_with_query
			: expires_relative;
	}


	void CopyFrom(AllocatorPtr alloc, const TranslateResponse &src) noexcept;

	/**
	 * Like CopyFrom(), but also copy fields that are excluded
	 * there.
	 */
	void FullCopyFrom(AllocatorPtr alloc, const TranslateResponse &src) noexcept;

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
	bool IsExpandable() const noexcept;

	/**
	 * Expand the strings in this response with the specified regex
	 * result.
	 *
	 * Throws std::runtime_error on error.
	 */
	void Expand(AllocatorPtr alloc, const MatchData &match_data);
#endif
};
