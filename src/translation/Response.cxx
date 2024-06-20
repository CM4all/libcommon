// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Response.hxx"
#if TRANSLATION_ENABLE_WIDGET
#include "widget/View.hxx"
#endif
#if TRANSLATION_ENABLE_CACHE
#include "uri/Base.hxx"
#include "uri/Compare.hxx"
#include "uri/PEscape.hxx"
#include "http/Status.hxx"
#include "HttpMessageResponse.hxx"
#endif
#include "AllocatorPtr.hxx"
#if TRANSLATION_ENABLE_EXPAND
#include "lib/pcre/UniqueRegex.hxx"
#include "pexpand.hxx"
#endif
#if TRANSLATION_ENABLE_SESSION
#include "http/Address.hxx"
#endif

#if TRANSLATION_ENABLE_RADDRESS
#include "translation/Layout.hxx"
#endif

void
TranslateResponse::Clear() noexcept
{
	protocol_version = 0;
	max_age = std::chrono::seconds(-1);
	expires_relative = std::chrono::seconds::zero();
	expires_relative_with_query = std::chrono::seconds::zero();
	status = HttpStatus{};

	token = nullptr;
	analytics_id = nullptr;
	generator = nullptr;

#if TRANSLATION_ENABLE_LOGIN
	no_password = nullptr;
	authorized_keys = nullptr;
#endif

#if TRANSLATION_ENABLE_EXECUTE
	shell = nullptr;
	execute = nullptr;
	child_options = ChildOptions();
#endif

#if TRANSLATION_ENABLE_RADDRESS
	address.Clear();
#endif

#if TRANSLATION_ENABLE_HTTP
	request_header_forward = HeaderForwardSettings::MakeDefaultRequest();
	response_header_forward = HeaderForwardSettings::MakeDefaultResponse();
#endif

#if TRANSLATION_ENABLE_RADDRESS
	base = nullptr;
	layout = {};
	layout_items.reset();
#endif

#if TRANSLATION_ENABLE_EXPAND
	regex = inverse_regex = nullptr;
#endif
	listener_tag = nullptr;
	site = nullptr;
	like_host = nullptr;
	canonical_host = nullptr;
#if TRANSLATION_ENABLE_HTTP
	document_root = nullptr;

	redirect = nullptr;
	bounce = nullptr;
#endif

	message = nullptr;

#if TRANSLATION_ENABLE_HTTP
	scheme = nullptr;
	host = nullptr;
	uri = nullptr;

	local_uri = nullptr;

	untrusted = nullptr;
	untrusted_prefix = nullptr;
	untrusted_site_suffix = nullptr;
	untrusted_raw_site_suffix = nullptr;
#endif

	test_path = nullptr;

	stats_tag = nullptr;

#if TRANSLATION_ENABLE_CACHE
	uncached = false;
	ignore_no_cache = false;
	eager_cache = false;
	auto_flush_cache = false;
#endif

#if TRANSLATION_ENABLE_RADDRESS
	unsafe_base = false;
	easy_base = false;
	path_exists = false;
#endif

#if TRANSLATION_ENABLE_EXPAND
	regex_tail = regex_raw = regex_unescape = inverse_regex_unescape = false;
	expand_site = false;
	expand_document_root = false;
	expand_redirect = false;
	expand_uri = false;
	expand_test_path = false;
	expand_auth_file = false;
	expand_cookie_host = false;
	expand_read_file = false;
#endif

#if TRANSLATION_ENABLE_WIDGET
	direct_addressing = false;
#endif
#if TRANSLATION_ENABLE_SESSION
	session_cookie_same_site = CookieSameSite::DEFAULT;
	stateful = false;
	discard_session = false;
	discard_realm_session = false;
	secure_cookie = false;
	require_csrf_token = false;
	send_csrf_token = false;
#endif
#if TRANSLATION_ENABLE_TRANSFORMATION
	filter_4xx = false;
#endif
	defer = false;
	previous = false;
	transparent = false;
#if TRANSLATION_ENABLE_HTTP
	discard_query_string = false;
	redirect_query_string = false;
	redirect_full_uri = false;
	https_only = 0;
#endif
#if TRANSLATION_ENABLE_RADDRESS
	auto_base = false;
#endif
#if TRANSLATION_ENABLE_WIDGET
	widget_info = false;
	anchor_absolute = false;
#endif
#if TRANSLATION_ENABLE_HTTP
	tiny_image = false;
	transparent_chain = false;
	break_chain = false;
	dump_headers = false;
#endif
#if TRANSLATION_ENABLE_EXPAND
	regex_on_host_uri = false;
	regex_on_user_uri = false;
#endif
#if TRANSLATION_ENABLE_RADDRESS
	auto_gzip = false;
	auto_brotli = false;
	auto_compress_only_text = false;
	auto_gzipped = false;
	auto_brotli_path = false;
#endif
#if TRANSLATION_ENABLE_LOGIN
	no_home_authorized_keys = false;
#endif
#if TRANSLATION_ENABLE_SESSION
	realm_from_auth_base = false;

	session = {};
	realm_session = {};
	attach_session = {};
#endif
	pool = nullptr;
#if TRANSLATION_ENABLE_HTTP
	internal_redirect = {};
	http_auth = {};
	token_auth = {};
#endif
#if TRANSLATION_ENABLE_SESSION
	check = {};
	check_header = nullptr;
	auth = {};
	auth_file = nullptr;
	append_auth = {};
	expand_append_auth = nullptr;
#endif

#if TRANSLATION_ENABLE_HTTP
	want_full_uri = {};
	chain = {};
#endif

	timeout = {};

#if TRANSLATION_ENABLE_SESSION
	session_site = nullptr;
	user = nullptr;
	user_max_age = std::chrono::seconds(-1);
	language = nullptr;
	realm = nullptr;

	external_session_manager = nullptr;
	external_session_keepalive = std::chrono::seconds::zero();

	www_authenticate = nullptr;
	authentication_info = nullptr;

	cookie_domain = cookie_host = cookie_path = nullptr;
	recover_session = nullptr;
#endif

#if TRANSLATION_ENABLE_HTTP
	request_headers.Clear();
	expand_request_headers.Clear();
	response_headers.Clear();
	expand_response_headers.Clear();
#endif

#if TRANSLATION_ENABLE_WIDGET
	views.clear();
	widget_group = nullptr;
	container_groups.Init();
#endif

#if TRANSLATION_ENABLE_CACHE
	cache_tag = nullptr;
	vary = {};
	invalidate = {};
#endif
#if TRANSLATION_ENABLE_WANT
	want = {};
#endif
#if TRANSLATION_ENABLE_RADDRESS
	file_not_found = {};
	content_type = nullptr;
	enotdir = {};
	directory_index = {};
#endif
	error_document = {};
	probe_path_suffixes = {};
	probe_suffixes = {};
	read_file = nullptr;

	validate_mtime.mtime = 0;
	validate_mtime.path = nullptr;
}

static std::span<const char *const>
DupStringArray(AllocatorPtr alloc, std::span<const char *const> src) noexcept
{
	if (src.empty())
		return {};

	const char **dest = alloc.NewArray<const char *>(src.size());

	const char **p = dest;
	for (const char *s : src)
		*p++ = alloc.Dup(s);

	return {dest, src.size()};
}

void
TranslateResponse::CopyFrom(AllocatorPtr alloc, const TranslateResponse &src) noexcept
{
	protocol_version = src.protocol_version;

	/* we don't copy the "max_age" attribute, because it's only used
	   by the tcache itself */

	expires_relative = src.expires_relative;
	expires_relative_with_query = src.expires_relative_with_query;

#if TRANSLATION_ENABLE_HTTP
	status = src.status;
#endif

	token = alloc.CheckDup(src.token);
	analytics_id = alloc.CheckDup(src.analytics_id);
	generator = alloc.CheckDup(src.generator);

#if TRANSLATION_ENABLE_LOGIN
	no_password = alloc.CheckDup(src.no_password);
	authorized_keys = alloc.CheckDup(src.authorized_keys);
#endif

#if TRANSLATION_ENABLE_EXECUTE
	shell = alloc.CheckDup(src.shell);
	execute = alloc.CheckDup(src.execute);
	args = ExpandableStringList(alloc, src.args);
	child_options = ChildOptions(alloc, src.child_options);
#endif

#if TRANSLATION_ENABLE_HTTP
	request_header_forward = src.request_header_forward;
	response_header_forward = src.response_header_forward;
#endif

#if TRANSLATION_ENABLE_RADDRESS
	base = alloc.CheckDup(src.base);
	layout = alloc.Dup(src.layout);
	layout_items = src.layout_items;
#endif

#if TRANSLATION_ENABLE_EXPAND
	regex = alloc.CheckDup(src.regex);
	inverse_regex = alloc.CheckDup(src.inverse_regex);
#endif
	listener_tag = alloc.CheckDup(src.listener_tag);
	site = alloc.CheckDup(src.site);
	like_host = alloc.CheckDup(src.like_host);
	canonical_host = alloc.CheckDup(src.canonical_host);
#if TRANSLATION_ENABLE_RADDRESS
	document_root = alloc.CheckDup(src.document_root);
	redirect = alloc.CheckDup(src.redirect);
	bounce = alloc.CheckDup(src.bounce);
#endif

	message = alloc.CheckDup(src.message);

#if TRANSLATION_ENABLE_RADDRESS
	scheme = alloc.CheckDup(src.scheme);
	host = alloc.CheckDup(src.host);
	uri = alloc.CheckDup(src.uri);
	local_uri = alloc.CheckDup(src.local_uri);
	untrusted = alloc.CheckDup(src.untrusted);
	untrusted_prefix = alloc.CheckDup(src.untrusted_prefix);
	untrusted_site_suffix =
		alloc.CheckDup(src.untrusted_site_suffix);
	untrusted_raw_site_suffix =
		alloc.CheckDup(src.untrusted_raw_site_suffix);
#endif

#if TRANSLATION_ENABLE_CACHE
	uncached = src.uncached;
	ignore_no_cache = src.ignore_no_cache;
	eager_cache = src.eager_cache;
	auto_flush_cache = src.auto_flush_cache;
#endif

#if TRANSLATION_ENABLE_RADDRESS
	unsafe_base = src.unsafe_base;
	easy_base = src.easy_base;
	path_exists = src.path_exists;
#endif

#if TRANSLATION_ENABLE_EXPAND
	regex_tail = src.regex_tail;
	regex_raw = src.regex_raw;
	regex_unescape = src.regex_unescape;
	inverse_regex_unescape = src.inverse_regex_unescape;
	expand_site = src.expand_site;
	expand_document_root = src.expand_document_root;
	expand_redirect = src.expand_redirect;
	expand_uri = src.expand_uri;
	expand_test_path = src.expand_test_path;
	expand_auth_file = src.expand_auth_file;
	expand_cookie_host = src.expand_cookie_host;
	expand_read_file = src.expand_read_file;
#endif

#if TRANSLATION_ENABLE_WIDGET
	direct_addressing = src.direct_addressing;
#endif
#if TRANSLATION_ENABLE_SESSION
	session_cookie_same_site = src.session_cookie_same_site;
	stateful = src.stateful;
	require_csrf_token = src.require_csrf_token;
	send_csrf_token = src.send_csrf_token;
	discard_session = src.discard_session;
	discard_realm_session = src.discard_realm_session;
	secure_cookie = src.secure_cookie;
#endif
#if TRANSLATION_ENABLE_TRANSFORMATION
	filter_4xx = src.filter_4xx;
#endif
	defer = src.defer;
	previous = src.previous;
	transparent = src.transparent;
#if TRANSLATION_ENABLE_HTTP
	discard_query_string = src.discard_query_string;
	redirect_query_string = src.redirect_query_string;
	redirect_full_uri = src.redirect_full_uri;
	https_only = src.https_only;
#endif
#if TRANSLATION_ENABLE_RADDRESS
	auto_base = src.auto_base;
#endif
#if TRANSLATION_ENABLE_WIDGET
	widget_info = src.widget_info;
	widget_group = alloc.CheckDup(src.widget_group);
#endif
	test_path = alloc.CheckDup(src.test_path);
	stats_tag = alloc.CheckDup(src.stats_tag);
#if TRANSLATION_ENABLE_SESSION
	auth_file = alloc.CheckDup(src.auth_file);
	append_auth = alloc.Dup(src.append_auth);
	expand_append_auth = alloc.CheckDup(src.expand_append_auth);
#endif

#if TRANSLATION_ENABLE_WIDGET
	container_groups.Init();
	container_groups.CopyFrom(alloc, src.container_groups);
#endif

#if TRANSLATION_ENABLE_WIDGET
	anchor_absolute = src.anchor_absolute;
#endif
#if TRANSLATION_ENABLE_HTTP
	tiny_image = src.tiny_image;
	transparent_chain = src.transparent_chain;
	break_chain = src.break_chain;
	dump_headers = src.dump_headers;
#endif
#if TRANSLATION_ENABLE_EXPAND
	regex_on_host_uri = src.regex_on_host_uri;
	regex_on_user_uri = src.regex_on_user_uri;
#endif
#if TRANSLATION_ENABLE_RADDRESS
	auto_gzip = src.auto_gzip;
	auto_brotli = src.auto_brotli;
	auto_compress_only_text = src.auto_compress_only_text;
	auto_gzipped = src.auto_gzipped;
	auto_brotli_path = src.auto_brotli_path;
#endif
#if TRANSLATION_ENABLE_LOGIN
	no_home_authorized_keys = src.no_home_authorized_keys;
#endif
#if TRANSLATION_ENABLE_SESSION
	realm_from_auth_base = src.realm_from_auth_base;
	session = {};
	realm_session = {};
	attach_session = {};
#endif

	pool = alloc.CheckDup(src.pool);

#if TRANSLATION_ENABLE_HTTP
	internal_redirect = alloc.Dup(src.internal_redirect);
	http_auth = alloc.Dup(src.http_auth);
	token_auth = alloc.Dup(src.token_auth);
#endif
#if TRANSLATION_ENABLE_SESSION
	check = alloc.Dup(src.check);
	check_header = alloc.CheckDup(src.check_header);
	auth = alloc.Dup(src.auth);
	want_full_uri = alloc.Dup(src.want_full_uri);
	chain = alloc.Dup(src.chain);
#endif

#if TRANSLATION_ENABLE_SESSION
	/* The "user" attribute must not be present in cached responses,
	   because they belong to only that one session.  For the same
	   reason, we won't copy the user_max_age attribute. */
	user = nullptr;
	session_site = nullptr;

	language = nullptr;
	realm = alloc.CheckDup(src.realm);

	external_session_manager = src.external_session_manager != nullptr
		? alloc.New<HttpAddress>(alloc, *src.external_session_manager)
		: nullptr;
	external_session_keepalive = src.external_session_keepalive;

	www_authenticate = alloc.CheckDup(src.www_authenticate);
	authentication_info = alloc.CheckDup(src.authentication_info);
	cookie_domain = alloc.CheckDup(src.cookie_domain);
	cookie_host = alloc.CheckDup(src.cookie_host);
	cookie_path = alloc.CheckDup(src.cookie_path);
	recover_session = alloc.CheckDup(src.recover_session);
#endif

#if TRANSLATION_ENABLE_HTTP
	request_headers = KeyValueList(alloc, src.request_headers);
	expand_request_headers = KeyValueList(alloc, src.expand_request_headers);
	response_headers = KeyValueList(alloc, src.response_headers);
	expand_response_headers = KeyValueList(alloc, src.expand_response_headers);
#endif

#if TRANSLATION_ENABLE_WIDGET
	views = Clone(alloc, src.views);
#endif

#if TRANSLATION_ENABLE_CACHE
	cache_tag = alloc.CheckDup(src.cache_tag);
	vary = alloc.Dup(src.vary);
	invalidate = alloc.Dup(src.invalidate);
#endif
#if TRANSLATION_ENABLE_WANT
	want = alloc.Dup(src.want);
#endif
#if TRANSLATION_ENABLE_RADDRESS
	file_not_found = alloc.Dup(src.file_not_found);
	content_type = alloc.CheckDup(src.content_type);
	enotdir = alloc.Dup(src.enotdir);
	directory_index = alloc.Dup(src.directory_index);
#endif
	error_document = alloc.Dup(src.error_document);
	probe_path_suffixes = alloc.Dup(src.probe_path_suffixes);
	probe_suffixes = DupStringArray(alloc, src.probe_suffixes);
	read_file = alloc.CheckDup(src.read_file);

	timeout = src.timeout;

	validate_mtime.mtime = src.validate_mtime.mtime;
	validate_mtime.path = alloc.CheckDup(src.validate_mtime.path);
}

void
TranslateResponse::FullCopyFrom(AllocatorPtr alloc, const TranslateResponse &src) noexcept
{
	CopyFrom(alloc, src);
	max_age = src.max_age;
#if TRANSLATION_ENABLE_RADDRESS
	address.CopyFrom(alloc, src.address);
#endif
#if TRANSLATION_ENABLE_SESSION
	user = alloc.CheckDup(src.user);
#endif
}

#if TRANSLATION_ENABLE_CACHE

void
TranslateResponse::CacheStore(AllocatorPtr alloc, const TranslateResponse &src,
			      const char *request_uri)
{
	CopyFrom(alloc, src);

	if (auto_base) {
		assert(base == nullptr);
		assert(request_uri != nullptr);

		base = src.address.AutoBase(alloc, request_uri);
	}

	const bool expandable = src.IsExpandable();

	if (src.address.IsDefined() || layout.data() == nullptr)
		address.CacheStore(alloc, src.address,
				   request_uri, base,
				   easy_base,
				   expandable);
	else
		address = nullptr;

	if (base != nullptr && !expandable && !easy_base) {
		const char *tail = base_tail(request_uri, base);
		if (tail != nullptr) {
			if (uri != nullptr) {
				size_t length = base_string(uri, tail);
				uri = length != (size_t)-1
					? alloc.DupZ({uri, length})
					: nullptr;

				if (uri == nullptr &&
				    internal_redirect.data() != nullptr)
					/* this BASE mismatch is
					   fatal, because it
					   invalidates a required
					   attribute */
					throw HttpMessageResponse(HttpStatus::BAD_GATEWAY,
								  "Base mismatch");
			}

			if (redirect != nullptr) {
				size_t length = base_string(redirect, tail);
				redirect = length != (size_t)-1
					? alloc.DupZ({redirect, length})
					: nullptr;
			}

			if (test_path != nullptr) {
				const char *end = UriFindUnescapedSuffix(test_path, tail);
				test_path = end != nullptr
					? alloc.DupZ({test_path, end})
					: nullptr;
			}
		}
	}
}

void
TranslateResponse::CacheLoad(AllocatorPtr alloc, const TranslateResponse &src,
			     const char *request_uri)
{
	const bool expandable = src.IsExpandable();

	address.CacheLoad(alloc, src.address, request_uri, src.base,
			  src.unsafe_base, expandable);

	if (this != &src)
		CopyFrom(alloc, src);

	if (base != nullptr && !expandable) {
		const char *tail = require_base_tail(request_uri, base);

		if (uri != nullptr)
			uri = alloc.Concat(uri, tail);

		if (redirect != nullptr)
			redirect = alloc.Concat(redirect, tail);

		if (test_path != nullptr) {
			char *unescaped = uri_unescape_dup(alloc, tail);
			if (unescaped == nullptr)
				throw HttpMessageResponse(HttpStatus::BAD_REQUEST,
							  "Malformed URI tail");

			test_path = alloc.Concat(test_path, unescaped);
		}
	}
}

#endif

#if TRANSLATION_ENABLE_EXPAND

UniqueRegex
TranslateResponse::CompileRegex() const
{
	assert(regex != nullptr);

	return {regex, {.anchored=protocol_version >= 3, .capture=IsExpandable()}};
}

UniqueRegex
TranslateResponse::CompileInverseRegex() const
{
	assert(inverse_regex != nullptr);

	return {inverse_regex, {.anchored=protocol_version >= 3}};
}

bool
TranslateResponse::IsExpandable() const noexcept
{
	return regex != nullptr &&
		(expand_redirect ||
		 expand_site ||
		 expand_document_root ||
		 expand_uri ||
		 expand_test_path ||
		 expand_auth_file ||
		 expand_read_file ||
		 expand_append_auth != nullptr ||
		 expand_cookie_host ||
		 !expand_request_headers.IsEmpty() ||
		 !expand_response_headers.IsEmpty() ||
		 address.IsExpandable() ||
		 (external_session_manager != nullptr &&
		  external_session_manager->IsExpandable()) ||
		 IsAnyExpandable(views));
}

void
TranslateResponse::Expand(AllocatorPtr alloc, const MatchData &match_data)
{
	assert(regex != nullptr);

	if (expand_redirect) {
		expand_redirect = false;
		redirect = expand_string_unescaped(alloc, redirect, match_data);
	}

	if (expand_site) {
		expand_site = false;
		site = expand_string_unescaped(alloc, site, match_data);
	}

	if (expand_document_root) {
		expand_document_root = false;
		document_root = expand_string_unescaped(alloc, document_root,
							match_data);
	}

	if (expand_uri) {
		expand_uri = false;
		uri = expand_string_unescaped(alloc, uri, match_data);
	}

	if (expand_test_path) {
		expand_test_path = false;
		test_path = expand_string_unescaped(alloc, test_path, match_data);
	}

	if (expand_auth_file) {
		expand_auth_file = false;
		auth_file = expand_string_unescaped(alloc, auth_file, match_data);
	}

	if (expand_read_file) {
		expand_read_file = false;
		read_file = expand_string_unescaped(alloc, read_file, match_data);
	}

	if (expand_append_auth != nullptr) {
		const char *value = expand_string_unescaped(alloc, expand_append_auth,
							    match_data);
		append_auth = { (const std::byte *)value, strlen(value) };
	}

	if (expand_cookie_host) {
		expand_cookie_host = false;
		cookie_host = expand_string_unescaped(alloc, cookie_host, match_data);
	}

	for (const auto &i : expand_request_headers) {
		const char *value = expand_string_unescaped(alloc, i.value, match_data);
		request_headers.Add(alloc, i.key, value);
	}

	for (const auto &i : expand_response_headers) {
		const char *value = expand_string_unescaped(alloc, i.value, match_data);
		response_headers.Add(alloc, i.key, value);
	}

	address.Expand(alloc, match_data);

	if (external_session_manager != nullptr)
		external_session_manager->Expand(alloc, match_data);

	::Expand(alloc, views, match_data);
}

#endif
