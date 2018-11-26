/*
 * Copyright 2007-2017 Content Management AG
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

#include "Response.hxx"
#if TRANSLATION_ENABLE_WIDGET
#include "widget/View.hxx"
#endif
#if TRANSLATION_ENABLE_CACHE
#include "uri/uri_base.hxx"
#include "uri/Compare.hxx"
#include "puri_escape.hxx"
#include "HttpMessageResponse.hxx"
#endif
#include "AllocatorPtr.hxx"
#if TRANSLATION_ENABLE_EXPAND
#include "regex.hxx"
#include "pexpand.hxx"
#endif
#if TRANSLATION_ENABLE_SESSION
#include "http_address.hxx"
#endif
#include "util/StringView.hxx"

void
TranslateResponse::Clear()
{
    protocol_version = 0;
    max_age = std::chrono::seconds(-1);
    expires_relative = std::chrono::seconds::zero();
#if TRANSLATION_ENABLE_HTTP
    status = (http_status_t)0;
#else
    status = 0;
#endif

    token = nullptr;

#if TRANSLATION_ENABLE_EXECUTE
    shell = nullptr;
    execute = nullptr;
    child_options = ChildOptions();
#endif

#if TRANSLATION_ENABLE_RADDRESS
    address.Clear();
#endif

#if TRANSLATION_ENABLE_HTTP
    using namespace BengProxy;

    request_header_forward = HeaderForwardSettings{
        .modes = {
            [(size_t)HeaderGroup::IDENTITY] = HeaderForwardMode::MANGLE,
            [(size_t)HeaderGroup::CAPABILITIES] = HeaderForwardMode::YES,
            [(size_t)HeaderGroup::COOKIE] = HeaderForwardMode::MANGLE,
            [(size_t)HeaderGroup::OTHER] = HeaderForwardMode::NO,
            [(size_t)HeaderGroup::FORWARD] = HeaderForwardMode::NO,
            [(size_t)HeaderGroup::CORS] = HeaderForwardMode::NO,
            [(size_t)HeaderGroup::SECURE] = HeaderForwardMode::NO,
            [(size_t)HeaderGroup::TRANSFORMATION] = HeaderForwardMode::NO,
            [(size_t)HeaderGroup::LINK] = HeaderForwardMode::NO,
            [(size_t)HeaderGroup::SSL] = HeaderForwardMode::NO,
            [(size_t)HeaderGroup::AUTH] = HeaderForwardMode::MANGLE,
        },
    };

    response_header_forward = HeaderForwardSettings{
        .modes = {
            [(size_t)HeaderGroup::IDENTITY] = HeaderForwardMode::NO,
            [(size_t)HeaderGroup::CAPABILITIES] = HeaderForwardMode::YES,
            [(size_t)HeaderGroup::COOKIE] = HeaderForwardMode::MANGLE,
            [(size_t)HeaderGroup::OTHER] = HeaderForwardMode::NO,
            [(size_t)HeaderGroup::FORWARD] = HeaderForwardMode::NO,
            [(size_t)HeaderGroup::CORS] = HeaderForwardMode::NO,
            [(size_t)HeaderGroup::SECURE] = HeaderForwardMode::NO,
            [(size_t)HeaderGroup::TRANSFORMATION] = HeaderForwardMode::MANGLE,
            [(size_t)HeaderGroup::LINK] = HeaderForwardMode::YES,
            [(size_t)HeaderGroup::SSL] = HeaderForwardMode::NO,
            [(size_t)HeaderGroup::AUTH] = HeaderForwardMode::MANGLE,
        },
    };
#endif

    base = nullptr;
#if TRANSLATION_ENABLE_EXPAND
    regex = inverse_regex = nullptr;
#endif
    site = nullptr;
    canonical_host = nullptr;
#if TRANSLATION_ENABLE_HTTP
    document_root = nullptr;

    redirect = nullptr;
    bounce = nullptr;

    message = nullptr;

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

    uncached = false;

#if TRANSLATION_ENABLE_RADDRESS
    unsafe_base = false;
    easy_base = false;
#endif

#if TRANSLATION_ENABLE_EXPAND
    regex_tail = regex_unescape = inverse_regex_unescape = false;
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
    stateful = false;
    discard_session = false;
    secure_cookie = false;
#endif
#if TRANSLATION_ENABLE_TRANSFORMATION
    filter_4xx = false;
#endif
    previous = false;
    transparent = false;
#if TRANSLATION_ENABLE_HTTP
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
    dump_headers = false;
#endif
#if TRANSLATION_ENABLE_EXPAND
    regex_on_host_uri = false;
    regex_on_user_uri = false;
#endif
    auto_deflate = false;
    auto_gzip = false;
#if TRANSLATION_ENABLE_SESSION
    realm_from_auth_base = false;

    session = nullptr;
#endif
    pool = nullptr;
#if TRANSLATION_ENABLE_HTTP
    internal_redirect = nullptr;
#endif
#if TRANSLATION_ENABLE_SESSION
    check = nullptr;
    auth = nullptr;
    auth_file = nullptr;
    append_auth = nullptr;
    expand_append_auth = nullptr;
#endif

#if TRANSLATION_ENABLE_HTTP
    want_full_uri = nullptr;
#endif

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
#endif

#if TRANSLATION_ENABLE_HTTP
    request_headers.Clear();
    expand_request_headers.Clear();
    response_headers.Clear();
    expand_response_headers.Clear();
#endif

#if TRANSLATION_ENABLE_WIDGET
    views = nullptr;
    widget_group = nullptr;
    container_groups.Init();
#endif

#if TRANSLATION_ENABLE_CACHE
    vary = nullptr;
    invalidate = nullptr;
#endif
#if TRANSLATION_ENABLE_WANT
    want = nullptr;
#endif
#if TRANSLATION_ENABLE_RADDRESS
    file_not_found = nullptr;
    content_type = nullptr;
    enotdir = nullptr;
    directory_index = nullptr;
#endif
    error_document = nullptr;
    probe_path_suffixes = nullptr;
    probe_suffixes = nullptr;
    read_file = nullptr;

    validate_mtime.mtime = 0;
    validate_mtime.path = nullptr;
}

static ConstBuffer<const char *>
DupStringArray(AllocatorPtr alloc, ConstBuffer<const char *> src) noexcept
{
    if (src.empty())
        return nullptr;

    const char **dest = alloc.NewArray<const char *>(src.size);

    const char **p = dest;
    for (const char *s : src)
        *p++ = alloc.Dup(s);

    return {dest, src.size};
}

void
TranslateResponse::CopyFrom(AllocatorPtr alloc, const TranslateResponse &src)
{
    protocol_version = src.protocol_version;

    /* we don't copy the "max_age" attribute, because it's only used
       by the tcache itself */

    expires_relative = src.expires_relative;

#if TRANSLATION_ENABLE_HTTP
    status = src.status;
#endif

    token = alloc.CheckDup(src.token);

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

    base = alloc.CheckDup(src.base);
#if TRANSLATION_ENABLE_EXPAND
    regex = alloc.CheckDup(src.regex);
    inverse_regex = alloc.CheckDup(src.inverse_regex);
#endif
    site = alloc.CheckDup(src.site);
    canonical_host = alloc.CheckDup(src.canonical_host);
#if TRANSLATION_ENABLE_RADDRESS
    document_root = alloc.CheckDup(src.document_root);
    redirect = alloc.CheckDup(src.redirect);
    bounce = alloc.CheckDup(src.bounce);
    message = alloc.CheckDup(src.message);
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

    uncached = src.uncached;

#if TRANSLATION_ENABLE_RADDRESS
    unsafe_base = src.unsafe_base;
    easy_base = src.easy_base;
#endif

#if TRANSLATION_ENABLE_EXPAND
    regex_tail = src.regex_tail;
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
    stateful = src.stateful;
    discard_session = src.discard_session;
    secure_cookie = src.secure_cookie;
#endif
#if TRANSLATION_ENABLE_TRANSFORMATION
    filter_4xx = src.filter_4xx;
#endif
    previous = src.previous;
    transparent = src.transparent;
#if TRANSLATION_ENABLE_HTTP
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
    dump_headers = src.dump_headers;
#endif
#if TRANSLATION_ENABLE_EXPAND
    regex_on_host_uri = src.regex_on_host_uri;
    regex_on_user_uri = src.regex_on_user_uri;
#endif
    auto_deflate = src.auto_deflate;
    auto_gzip = src.auto_gzip;
#if TRANSLATION_ENABLE_SESSION
    realm_from_auth_base = src.realm_from_auth_base;
    session = nullptr;
#endif

    pool = alloc.CheckDup(src.pool);

#if TRANSLATION_ENABLE_HTTP
    internal_redirect = alloc.Dup(src.internal_redirect);
#endif
#if TRANSLATION_ENABLE_SESSION
    check = alloc.Dup(src.check);
    auth = alloc.Dup(src.auth);
    want_full_uri = alloc.Dup(src.want_full_uri);
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
#endif

#if TRANSLATION_ENABLE_HTTP
    request_headers = KeyValueList(alloc, src.request_headers);
    expand_request_headers = KeyValueList(alloc, src.expand_request_headers);
    response_headers = KeyValueList(alloc, src.response_headers);
    expand_response_headers = KeyValueList(alloc, src.expand_response_headers);
#endif

#if TRANSLATION_ENABLE_WIDGET
    views = src.views != nullptr
        ? src.views->CloneChain(alloc)
        : nullptr;
#endif

#if TRANSLATION_ENABLE_CACHE
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

    validate_mtime.mtime = src.validate_mtime.mtime;
    validate_mtime.path = alloc.CheckDup(src.validate_mtime.path);
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

    address.CacheStore(alloc, src.address,
                       request_uri, base,
                       easy_base,
                       expandable);

    if (base != nullptr && !expandable && !easy_base) {
        const char *tail = base_tail(request_uri, base);
        if (tail != nullptr) {
            if (uri != nullptr) {
                size_t length = base_string(uri, tail);
                uri = length != (size_t)-1
                    ? alloc.DupZ({uri, length})
                    : nullptr;

                if (uri == nullptr && !internal_redirect.IsNull())
                    /* this BASE mismatch is fatal, because it
                       invalidates a required attribute; clearing
                       "base" is an trigger for tcache_store() to fail
                       on this translation response */
                    // TODO: throw an exception instead of this kludge
                    base = nullptr;
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
                throw HttpMessageResponse(HTTP_STATUS_BAD_REQUEST, "Malformed URI tail");

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

    return {regex, protocol_version >= 3, IsExpandable()};
}

UniqueRegex
TranslateResponse::CompileInverseRegex() const
{
    assert(inverse_regex != nullptr);

    return {inverse_regex, protocol_version >= 3, false};
}

bool
TranslateResponse::IsExpandable() const
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
         widget_view_any_is_expandable(views));
}

void
TranslateResponse::Expand(AllocatorPtr alloc, const MatchInfo &match_info)
{
    assert(regex != nullptr);

    if (expand_redirect) {
        expand_redirect = false;
        redirect = expand_string_unescaped(alloc, redirect, match_info);
    }

    if (expand_site) {
        expand_site = false;
        site = expand_string_unescaped(alloc, site, match_info);
    }

    if (expand_document_root) {
        expand_document_root = false;
        document_root = expand_string_unescaped(alloc, document_root,
                                                match_info);
    }

    if (expand_uri) {
        expand_uri = false;
        uri = expand_string_unescaped(alloc, uri, match_info);
    }

    if (expand_test_path) {
        expand_test_path = false;
        test_path = expand_string_unescaped(alloc, test_path, match_info);
    }

    if (expand_auth_file) {
        expand_auth_file = false;
        auth_file = expand_string_unescaped(alloc, auth_file, match_info);
    }

    if (expand_read_file) {
        expand_read_file = false;
        read_file = expand_string_unescaped(alloc, read_file, match_info);
    }

    if (expand_append_auth != nullptr) {
        const char *value = expand_string_unescaped(alloc, expand_append_auth,
                                                    match_info);
        append_auth = { value, strlen(value) };
    }

    if (expand_cookie_host) {
        expand_cookie_host = false;
        cookie_host = expand_string_unescaped(alloc, cookie_host, match_info);
    }

    for (const auto &i : expand_request_headers) {
        const char *value = expand_string_unescaped(alloc, i.value, match_info);
        request_headers.Add(alloc, i.key, value);
    }

    for (const auto &i : expand_response_headers) {
        const char *value = expand_string_unescaped(alloc, i.value, match_info);
        response_headers.Add(alloc, i.key, value);
    }

    address.Expand(alloc, match_info);

    if (external_session_manager != nullptr)
        external_session_manager->Expand(alloc, match_info);

    widget_view_expand_all(alloc, views, match_info);
}

#endif
