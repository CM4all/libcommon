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

#include "Parser.hxx"
#if TRANSLATION_ENABLE_TRANSFORMATION
#include "translation/Transformation.hxx"
#include "bp/XmlProcessor.hxx"
#include "bp/CssProcessor.hxx"
#endif
#if TRANSLATION_ENABLE_WIDGET
#include "widget/Class.hxx"
#endif
#if TRANSLATION_ENABLE_RADDRESS
#include "file_address.hxx"
#include "delegate/Address.hxx"
#include "lhttp_address.hxx"
#include "http_address.hxx"
#include "cgi_address.hxx"
#include "nfs/Address.hxx"
#endif
#include "spawn/ChildOptions.hxx"
#include "spawn/MountList.hxx"
#include "spawn/ResourceLimits.hxx"
#if TRANSLATION_ENABLE_JAILCGI
#include "spawn/JailParams.hxx"
#endif
#if TRANSLATION_ENABLE_HTTP
#include "net/AllocatedSocketAddress.hxx"
#include "net/AddressInfo.hxx"
#include "net/Parser.hxx"
#endif
#include "util/CharUtil.hxx"
#include "util/RuntimeError.hxx"
#include "util/StringCompare.hxx"

#if TRANSLATION_ENABLE_HTTP
#include "http/HeaderName.hxx"
#endif

#include <assert.h>
#include <string.h>

void
TranslateParser::SetChildOptions(ChildOptions &_child_options)
{
    child_options = &_child_options;
    ns_options = &child_options->ns;
    mount_list = &ns_options->mount.mounts;
    jail = nullptr;
    env_builder = child_options->env;
}

#if TRANSLATION_ENABLE_RADDRESS

void
TranslateParser::SetCgiAddress(ResourceAddress::Type type,
                               const char *path)
{
    cgi_address = alloc.New<CgiAddress>(path);

    *resource_address = ResourceAddress(type, *cgi_address);

    args_builder = cgi_address->args;
    params_builder = cgi_address->params;
    SetChildOptions(cgi_address->options);
}

#endif

/*
 * receive response
 *
 */

gcc_pure
static bool
HasNullByte(ConstBuffer<void> p) noexcept
{
    return memchr(p.data, 0, p.size) != nullptr;
}

gcc_pure
static bool
IsValidNonEmptyString(StringView s) noexcept
{
    return !s.empty() > 0 && !HasNullByte(s.ToVoid());
}

static constexpr bool
IsValidNameChar(char ch)
{
    return IsAlphaNumericASCII(ch) || ch == '-' || ch == '_';
}

gcc_pure
static bool
IsValidName(StringView s)
{
    if (s.empty())
        return false;

    for (char i : s)
        if (!IsValidNameChar(i))
            return false;

    return true;
}

gcc_pure
static bool
IsValidAbsolutePath(const char *p) noexcept
{
    return *p == '/';
}

gcc_pure
static bool
IsValidAbsolutePath(StringView p) noexcept
{
    return IsValidNonEmptyString(p) && IsValidAbsolutePath(p.data);
}

#if TRANSLATION_ENABLE_HTTP

gcc_pure
static bool
IsValidAbsoluteUriPath(StringView p) noexcept
{
    return IsValidAbsolutePath(p);
}

#endif

#if TRANSLATION_ENABLE_TRANSFORMATION

template<>
Transformation *
TranslateParser::AddTransformation(Transformation::Type type) noexcept
{
    auto t = alloc.New<Transformation>(type);

    transformation = t;
    *transformation_tail = t;
    transformation_tail = &t->next;

    return t;
}

ResourceAddress *
TranslateParser::AddFilter()
{
    auto *t = AddTransformation(Transformation::Type::FILTER);
    t->u.filter.address = nullptr;
    t->u.filter.reveal_user = false;
    return &t->u.filter.address;
}

void
TranslateParser::AddSubstYamlFile(const char *prefix,
                                  const char *file_path,
                                  const char *map_path) noexcept
{
    auto *t = AddTransformation(Transformation::Type::SUBST);
    t->u.subst.prefix = prefix;
    t->u.subst.yaml_file = file_path;
    t->u.subst.yaml_map_path = map_path;
}

#endif

#if TRANSLATION_ENABLE_HTTP

static void
parse_address_string(AllocatorPtr alloc, AddressList *list,
                     const char *p, int default_port)
{
    list->Add(alloc, ParseSocketAddress(p, default_port, false));
}

#endif

static bool
valid_view_name_char(char ch)
{
    return IsAlphaNumericASCII(ch) || ch == '_' || ch == '-';
}

static bool
valid_view_name(const char *name)
{
    assert(name != nullptr);

    do {
        if (!valid_view_name_char(*name))
            return false;
    } while (*++name != 0);

    return true;
}

#if TRANSLATION_ENABLE_WIDGET

void
TranslateParser::FinishView()
{
    assert(response.views != nullptr);

    WidgetView *v = view;
    if (view == nullptr) {
        v = response.views;
        assert(v != nullptr);

        const ResourceAddress *address = &response.address;
        if (address->IsDefined() && !v->address.IsDefined()) {
            /* no address yet: copy address from response */
            v->address.CopyFrom(alloc, *address);
            v->filter_4xx = response.filter_4xx;
        }

        v->request_header_forward = response.request_header_forward;
        v->response_header_forward = response.response_header_forward;
    } else {
        if (!v->address.IsDefined() && v != response.views)
            /* no address yet: inherits settings from the default view */
            v->InheritFrom(alloc, *response.views);
    }

    v->address.Check();
}

inline void
TranslateParser::AddView(const char *name)
{
    FinishView();

    auto new_view = alloc.New<WidgetView>(name);
    new_view->request_header_forward = response.request_header_forward;
    new_view->response_header_forward = response.response_header_forward;

    view = new_view;
    *widget_view_tail = new_view;
    widget_view_tail = &new_view->next;
    resource_address = &new_view->address;
    jail = nullptr;
    child_options = nullptr;
    ns_options = nullptr;
    mount_list = nullptr;
    file_address = nullptr;
    http_address = nullptr;
    cgi_address = nullptr;
    nfs_address = nullptr;
    lhttp_address = nullptr;
    address_list = nullptr;
    transformation_tail = &new_view->transformation;
    transformation = nullptr;
}

#endif

#if TRANSLATION_ENABLE_HTTP

static void
parse_header_forward(HeaderForwardSettings *settings,
                     ConstBuffer<void> payload)
{
    using namespace BengProxy;
    const auto *packet =
        (const HeaderForwardPacket *)payload.data;

    if (payload.size % sizeof(*packet) != 0)
        throw std::runtime_error("malformed header forward packet");

    while (payload.size > 0) {
        if (packet->group < int(HeaderGroup::ALL) ||
            packet->group >= int(HeaderGroup::MAX) ||
            (packet->mode != unsigned(HeaderForwardMode::NO) &&
             packet->mode != unsigned(HeaderForwardMode::YES) &&
             packet->mode != unsigned(HeaderForwardMode::BOTH) &&
             packet->mode != unsigned(HeaderForwardMode::MANGLE)) ||
            packet->reserved != 0)
            throw std::runtime_error("malformed header forward packet");

        if (HeaderGroup(packet->group) == HeaderGroup::ALL) {
            for (unsigned i = 0; i < unsigned(HeaderGroup::MAX); ++i)
                if (HeaderGroup(i) != HeaderGroup::SECURE &&
                    HeaderGroup(i) != HeaderGroup::AUTH &&
                    HeaderGroup(i) != HeaderGroup::SSL)
                    settings->modes[i] = HeaderForwardMode(packet->mode);
        } else
            settings->modes[packet->group] = HeaderForwardMode(packet->mode);

        ++packet;
        payload.size -= sizeof(*packet);
    }
}

static void
parse_header(AllocatorPtr alloc,
             KeyValueList &headers, const char *packet_name,
             StringView payload)
{
    const char *value = payload.Find(':');
    if (value == nullptr || value == payload.data ||
        HasNullByte(payload.ToVoid()))
        throw FormatRuntimeError("malformed %s packet", packet_name);

    const char *name = alloc.DupToLower(StringView(payload.data, value));
    ++value;

    if (!http_header_name_valid(name))
        throw FormatRuntimeError("malformed name in %s packet", packet_name);
    else if (http_header_is_hop_by_hop(name))
        throw FormatRuntimeError("hop-by-hop %s packet", packet_name);

    headers.Add(alloc, name, value);
}

#endif

#if TRANSLATION_ENABLE_JAILCGI

/**
 * Throws std::runtime_error on error.
 */
static void
FinishJailParams(JailParams *jail,
                 const TranslateResponse &response,
                 const char *document_root)
{
    if (jail == nullptr || !jail->enabled)
        return;

    if (jail->home_directory == nullptr)
        jail->home_directory = document_root;

    if (jail->home_directory == nullptr)
        throw std::runtime_error("No home directory for JAIL");

    if (jail->site_id == nullptr)
        jail->site_id = response.site;
}

#endif

/**
 * Final fixups for the response before it is passed to the handler.
 *
 * Throws std::runtime_error on error.
 */
static void
FinishTranslateResponse(AllocatorPtr alloc,
                        TranslateResponse &response,
                        ConstBuffer<const char *> probe_suffixes)
{
#if TRANSLATION_ENABLE_RADDRESS
    if (response.easy_base && !response.address.IsValidBase())
        /* EASY_BASE was enabled, but the resource address does not
           end with a slash, thus LoadBase() cannot work */
        throw std::runtime_error("Invalid base address");

    if (response.address.IsCgiAlike()) {
        auto &cgi = response.address.GetCgi();

        if (cgi.uri == nullptr) {
            cgi.uri = response.uri;
            cgi.expand_uri = response.expand_uri;
        }

        if (cgi.document_root == nullptr) {
            cgi.document_root = response.document_root;
            cgi.expand_document_root = response.expand_document_root;
        }

        FinishJailParams(cgi.options.jail,
                         response, cgi.document_root);
    } else if (response.address.type == ResourceAddress::Type::LOCAL) {
        auto &file = response.address.GetFile();

        if (file.delegate != nullptr) {
            if (file.delegate->child_options.jail != nullptr &&
                file.delegate->child_options.jail->enabled &&
                file.document_root == nullptr)
                file.document_root = response.document_root;

            FinishJailParams(file.delegate->child_options.jail,
                             response,
                             file.document_root);
        }
    }

    response.address.Check();
#endif

#if TRANSLATION_ENABLE_HTTP
    /* these lists are in reverse order because new items were added
       to the front; reverse them now */
    response.request_headers.Reverse();
    response.response_headers.Reverse();
#endif

    if (!probe_suffixes.IsNull())
        response.probe_suffixes = alloc.Dup(probe_suffixes);

    if (!response.probe_path_suffixes.IsNull() &&
        response.probe_suffixes.empty())
        throw std::runtime_error("PROBE_PATH_SUFFIX without PROBE_SUFFIX");

#if TRANSLATION_ENABLE_HTTP
    if (!response.internal_redirect.IsNull() && response.uri == nullptr)
        throw std::runtime_error("INTERNAL_REDIRECT without URI");

    if (!response.internal_redirect.IsNull() &&
        !response.want_full_uri.IsNull())
        throw std::runtime_error("INTERNAL_REDIRECT conflicts with WANT_FULL_URI");
#endif
}

gcc_pure
static bool
translate_client_check_pair(StringView payload) noexcept
{
    return !payload.empty() && payload.front() != '=' &&
        !HasNullByte(payload.ToVoid()) &&
        strchr(payload.data + 1, '=') != nullptr;
}

static void
translate_client_check_pair(const char *name, StringView payload)
{
    if (!translate_client_check_pair(payload))
        throw FormatRuntimeError("malformed %s packet", name);
}

static void
translate_client_pair(AllocatorPtr alloc,
                      ExpandableStringList::Builder &builder,
                      const char *name, StringView payload)
{
    translate_client_check_pair(name, payload);

    builder.Add(alloc, payload.data, false);
}

#if TRANSLATION_ENABLE_EXPAND

static void
translate_client_expand_pair(ExpandableStringList::Builder &builder,
                             const char *name,
                             StringView payload)
{
    if (!builder.CanSetExpand())
        throw FormatRuntimeError("misplaced %s packet", name);

    translate_client_check_pair(name, payload);

    builder.SetExpand(payload.data);
}

#endif

static void
translate_client_pivot_root(NamespaceOptions *ns, StringView payload)
{
    if (!IsValidAbsolutePath(payload))
        throw std::runtime_error("malformed PIVOT_ROOT packet");

    if (ns == nullptr || ns->mount.pivot_root != nullptr ||
        ns->mount.mount_root_tmpfs)
        throw std::runtime_error("misplaced PIVOT_ROOT packet");

    ns->mount.enable_mount = true;
    ns->mount.pivot_root = payload.data;
}

static void
translate_client_mount_root_tmpfs(NamespaceOptions *ns,
                                  size_t payload_length)
{
    if (payload_length > 0)
        throw std::runtime_error("malformed MOUNT_ROOT_TMPFS packet");

    if (ns == nullptr || ns->mount.pivot_root != nullptr ||
        ns->mount.mount_root_tmpfs)
        throw std::runtime_error("misplaced MOUNT_ROOT_TMPFS packet");

    ns->mount.enable_mount = true;
    ns->mount.mount_root_tmpfs = true;
}

static void
translate_client_home(NamespaceOptions *ns,
#if TRANSLATION_ENABLE_JAILCGI
                      JailParams *jail,
#endif
                      StringView payload)
{
    if (!IsValidAbsolutePath(payload))
        throw std::runtime_error("malformed HOME packet");

    bool ok = false;

    if (ns != nullptr && ns->mount.home == nullptr) {
        ns->mount.home = payload.data;
        ok = true;
    }

#if TRANSLATION_ENABLE_JAILCGI
    if (jail != nullptr && jail->enabled && jail->home_directory == nullptr) {
        jail->home_directory = payload.data;
        ok = true;
    }
#endif

    if (!ok)
        throw std::runtime_error("misplaced HOME packet");
}

#if TRANSLATION_ENABLE_EXPAND

static void
translate_client_expand_home(NamespaceOptions *ns,
#if TRANSLATION_ENABLE_JAILCGI
                             JailParams *jail,
#endif
                             StringView payload)
{
    if (!IsValidAbsolutePath(payload))
        throw std::runtime_error("malformed EXPAND_HOME packet");

    bool ok = false;

    if (ns != nullptr && ns->mount.expand_home == nullptr) {
        ns->mount.expand_home = payload.data;
        ok = true;
    }

#if TRANSLATION_ENABLE_JAILCGI
    if (jail != nullptr && jail->enabled &&
        !jail->expand_home_directory) {
        jail->home_directory = payload.data;
        jail->expand_home_directory = true;
        ok = true;
    }
#endif

    if (!ok)
        throw std::runtime_error("misplaced EXPAND_HOME packet");
}

#endif

static void
translate_client_mount_proc(NamespaceOptions *ns,
                            size_t payload_length)
{
    if (payload_length > 0)
        throw std::runtime_error("malformed MOUNT_PROC packet");

    if (ns == nullptr || ns->mount.mount_proc)
        throw std::runtime_error("misplaced MOUNT_PROC packet");

    ns->mount.enable_mount = true;
    ns->mount.mount_proc = true;
}

static void
translate_client_mount_tmp_tmpfs(NamespaceOptions *ns,
                                 StringView payload)
{
    if (HasNullByte(payload.ToVoid()))
        throw std::runtime_error("malformed MOUNT_TMP_TMPFS packet");

    if (ns == nullptr || ns->mount.mount_tmp_tmpfs != nullptr)
        throw std::runtime_error("misplaced MOUNT_TMP_TMPFS packet");

    ns->mount.enable_mount = true;
    ns->mount.mount_tmp_tmpfs = payload.data != nullptr
        ? payload.data
        : "";
}

static void
translate_client_mount_home(NamespaceOptions *ns, StringView payload)
{
    if (!IsValidAbsolutePath(payload))
        throw std::runtime_error("malformed MOUNT_HOME packet");

    if (ns == nullptr || ns->mount.home == nullptr ||
        ns->mount.mount_home != nullptr)
        throw std::runtime_error("misplaced MOUNT_HOME packet");

    ns->mount.enable_mount = true;
    ns->mount.mount_home = payload.data;
}

static void
translate_client_mount_tmpfs(NamespaceOptions *ns, StringView payload)
{
    if (!IsValidAbsolutePath(payload) ||
        /* not allowed for /tmp, use MOUNT_TMP_TMPFS
           instead! */
        payload.Equals("/tmp"))
        throw std::runtime_error("malformed MOUNT_TMPFS packet");

    if (ns == nullptr || ns->mount.mount_tmpfs != nullptr)
        throw std::runtime_error("misplaced MOUNT_TMPFS packet");

    ns->mount.enable_mount = true;
    ns->mount.mount_tmpfs = payload.data;
}

inline void
TranslateParser::HandleBindMount(StringView payload,
                                 bool expand, bool writable, bool exec)
{
    if (payload.empty() || payload.front() != '/')
        throw std::runtime_error("malformed BIND_MOUNT packet");

    const char *separator = payload.Find('\0');
    if (separator == nullptr || separator[1] != '/')
        throw std::runtime_error("malformed BIND_MOUNT packet");

    if (mount_list == nullptr)
        throw std::runtime_error("misplaced BIND_MOUNT packet");

    auto *m = alloc.New<MountList>(/* skip the slash to make it relative */
                                   payload.data + 1,
                                   separator + 1,
                                   expand, writable, exec);
    *mount_list = m;
    mount_list = &m->next;
}

static void
translate_client_uts_namespace(NamespaceOptions *ns,
                               StringView payload)
{
    if (!IsValidNonEmptyString(payload))
        throw std::runtime_error("malformed MOUNT_UTS_NAMESPACE packet");

    if (ns == nullptr || ns->hostname != nullptr)
        throw std::runtime_error("misplaced MOUNT_UTS_NAMESPACE packet");

    ns->hostname = payload.data;
}

static void
translate_client_rlimits(AllocatorPtr alloc, ChildOptions *child_options,
                         const char *payload)
{
    if (child_options == nullptr)
        throw std::runtime_error("misplaced RLIMITS packet");

    if (child_options->rlimits == nullptr)
        child_options->rlimits = alloc.New<ResourceLimits>();

    if (!child_options->rlimits->Parse(payload))
        throw std::runtime_error("malformed RLIMITS packet");
}

#if TRANSLATION_ENABLE_WANT

inline void
TranslateParser::HandleWant(const TranslationCommand *payload,
                            size_t payload_length)
{
    if (response.protocol_version < 1)
        throw std::runtime_error("WANT requires protocol version 1");

    if (from_request.want)
        throw std::runtime_error("WANT loop");

    if (!response.want.empty())
        throw std::runtime_error("duplicate WANT packet");

    if (payload_length % sizeof(payload[0]) != 0)
        throw std::runtime_error("malformed WANT packet");

    response.want = { payload, payload_length / sizeof(payload[0]) };
}

#endif

#if TRANSLATION_ENABLE_RADDRESS

static void
translate_client_file_not_found(TranslateResponse &response,
                                ConstBuffer<void> payload)
{
    if (!response.file_not_found.IsNull())
        throw std::runtime_error("duplicate FILE_NOT_FOUND packet");

    if (response.test_path == nullptr) {
        switch (response.address.type) {
        case ResourceAddress::Type::NONE:
            throw std::runtime_error("FIlE_NOT_FOUND without resource address");

        case ResourceAddress::Type::HTTP:
        case ResourceAddress::Type::PIPE:
            throw std::runtime_error("FIlE_NOT_FOUND not compatible with resource address");

        case ResourceAddress::Type::LOCAL:
        case ResourceAddress::Type::NFS:
        case ResourceAddress::Type::CGI:
        case ResourceAddress::Type::FASTCGI:
        case ResourceAddress::Type::WAS:
        case ResourceAddress::Type::LHTTP:
            break;
        }
    }

    response.file_not_found = payload;
}

inline void
TranslateParser::HandleContentTypeLookup(ConstBuffer<void> payload)
{
    const char *content_type;
    ConstBuffer<void> *content_type_lookup;

    if (file_address != nullptr) {
        content_type = file_address->content_type;
        content_type_lookup = &file_address->content_type_lookup;
    } else if (nfs_address != nullptr) {
        content_type = nfs_address->content_type;
        content_type_lookup = &nfs_address->content_type_lookup;
    } else
        throw std::runtime_error("misplaced CONTENT_TYPE_LOOKUP");

    if (!content_type_lookup->IsNull())
        throw std::runtime_error("duplicate CONTENT_TYPE_LOOKUP");

    if (content_type != nullptr)
        throw std::runtime_error("CONTENT_TYPE/CONTENT_TYPE_LOOKUP conflict");

    *content_type_lookup = payload;
}

static void
translate_client_enotdir(TranslateResponse &response,
                         ConstBuffer<void> payload)
{
    if (!response.enotdir.IsNull())
        throw std::runtime_error("duplicate ENOTDIR");

    if (response.test_path == nullptr) {
        switch (response.address.type) {
        case ResourceAddress::Type::NONE:
            throw std::runtime_error("ENOTDIR without resource address");

        case ResourceAddress::Type::HTTP:
        case ResourceAddress::Type::PIPE:
        case ResourceAddress::Type::NFS:
            throw std::runtime_error("ENOTDIR not compatible with resource address");

        case ResourceAddress::Type::LOCAL:
        case ResourceAddress::Type::CGI:
        case ResourceAddress::Type::FASTCGI:
        case ResourceAddress::Type::WAS:
        case ResourceAddress::Type::LHTTP:
            break;
        }
    }

    response.enotdir = payload;
}

static void
translate_client_directory_index(TranslateResponse &response,
                                 ConstBuffer<void> payload)
{
    if (!response.directory_index.IsNull())
        throw std::runtime_error("duplicate DIRECTORY_INDEX");

    if (response.test_path == nullptr) {
        switch (response.address.type) {
        case ResourceAddress::Type::NONE:
            throw std::runtime_error("DIRECTORY_INDEX without resource address");

        case ResourceAddress::Type::HTTP:
        case ResourceAddress::Type::LHTTP:
        case ResourceAddress::Type::PIPE:
        case ResourceAddress::Type::CGI:
        case ResourceAddress::Type::FASTCGI:
        case ResourceAddress::Type::WAS:
            throw std::runtime_error("DIRECTORY_INDEX not compatible with resource address");

        case ResourceAddress::Type::LOCAL:
        case ResourceAddress::Type::NFS:
            break;
        }
    }

    response.directory_index = payload;
}

#endif

static void
translate_client_expires_relative(TranslateResponse &response,
                                  ConstBuffer<void> payload)
{
    if (response.expires_relative > std::chrono::seconds::zero())
        throw std::runtime_error("duplicate EXPIRES_RELATIVE");

    if (payload.size != sizeof(uint32_t))
        throw std::runtime_error("malformed EXPIRES_RELATIVE");

    response.expires_relative = std::chrono::seconds(*(const uint32_t *)payload.data);
}

static void
translate_client_stderr_path(ChildOptions *child_options,
                             ConstBuffer<void> payload,
                             bool jailed)
{
    const StringView path(payload);
    if (!IsValidAbsolutePath(path))
        throw std::runtime_error("malformed STDERR_PATH packet");

    if (child_options == nullptr || child_options->stderr_null)
        throw std::runtime_error("misplaced STDERR_PATH packet");

    if (child_options->stderr_path != nullptr)
        throw std::runtime_error("duplicate STDERR_PATH packet");

    child_options->stderr_path = path.data;
    child_options->stderr_jailed = jailed;
}

#if TRANSLATION_ENABLE_EXPAND

static void
translate_client_expand_stderr_path(ChildOptions *child_options,
                                    StringView payload)
{
    if (!IsValidNonEmptyString(payload))
        throw std::runtime_error("malformed EXPAND_STDERR_PATH packet");

    if (child_options == nullptr)
        throw std::runtime_error("misplaced EXPAND_STDERR_PATH packet");

    if (child_options->expand_stderr_path != nullptr)
        throw std::runtime_error("duplicate EXPAND_STDERR_PATH packet");

    child_options->expand_stderr_path = payload.data;
}

#endif

gcc_pure
static bool
CheckRefence(StringView payload)
{
    auto p = payload.begin();
    const auto end = payload.end();

    while (true) {
        auto n = std::find(p, end, '\0');
        if (n == p)
            return false;

        if (n == end)
            return true;

        p = n + 1;
    }
}

inline void
TranslateParser::HandleRefence(StringView payload)
{
    if (child_options == nullptr || !child_options->refence.IsEmpty())
        throw std::runtime_error("misplaced REFENCE packet");

    if (!CheckRefence(payload))
        throw std::runtime_error("malformed REFENCE packet");

    child_options->refence.Set(payload);
}

inline void
TranslateParser::HandleUidGid(ConstBuffer<void> _payload)
{
    if (child_options == nullptr || !child_options->uid_gid.IsEmpty())
        throw std::runtime_error("misplaced UID_GID packet");

    UidGid &uid_gid = child_options->uid_gid;

    constexpr size_t min_size = sizeof(int) * 2;
    const size_t max_size = min_size + sizeof(int) * uid_gid.groups.max_size();

    if (_payload.size < min_size || _payload.size > max_size ||
        _payload.size % sizeof(int) != 0)
        throw std::runtime_error("malformed UID_GID packet");

    const auto payload = ConstBuffer<int>::FromVoid(_payload);
    uid_gid.uid = payload[0];
    uid_gid.gid = payload[1];

    size_t n_groups = payload.size - 2;
    std::copy(std::next(payload.begin(), 2), payload.end(),
              uid_gid.groups.begin());
    if (n_groups < uid_gid.groups.max_size())
        uid_gid.groups[n_groups] = 0;
}

inline void
TranslateParser::HandleUmask(ConstBuffer<void> payload)
{
    typedef uint16_t value_type;

    if (child_options == nullptr)
        throw std::runtime_error("misplaced UMASK packet");

    if (child_options->umask >= 0)
        throw std::runtime_error("duplicate UMASK packet");

    if (payload.size != sizeof(value_type))
        throw std::runtime_error("malformed UMASK packet");

    auto umask = *(const uint16_t *)payload.data;
    if (umask & ~0777)
        throw std::runtime_error("malformed UMASK packet");

    child_options->umask = umask;
}

gcc_pure
static bool
IsValidCgroupSetName(StringView name)
{
    const char *dot = name.Find('.');
    if (dot == nullptr || dot == name.begin() || dot == name.end())
        return false;

    const StringView controller(name.data, dot);

    for (char ch : controller)
        if (!IsLowerAlphaASCII(ch) && ch != '_')
            return false;

    if (controller.Equals("cgroup"))
        /* this is not a controller, this is a core cgroup
           attribute */
        return false;

    const StringView attribute(dot + 1, name.end());

    for (char ch : attribute)
        if (!IsLowerAlphaASCII(ch) && ch != '.' && ch != '_')
            return false;

    return true;
}

gcc_pure
static bool
IsValidCgroupSetValue(StringView value)
{
    return !value.empty() && value.Find('/') == nullptr;
}

gcc_pure
static std::pair<StringView, StringView>
ParseCgroupSet(StringView payload)
{
    if (HasNullByte(payload.ToVoid()))
        return std::make_pair(nullptr, nullptr);

    const char *eq = payload.Find('=');
    if (eq == nullptr)
        return std::make_pair(nullptr, nullptr);

    StringView name(payload.data, eq), value(eq + 1, payload.end());
    if (!IsValidCgroupSetName(name) || !IsValidCgroupSetValue(value))
        return std::make_pair(nullptr, nullptr);

    return std::make_pair(name, value);
}

inline void
TranslateParser::HandleCgroupSet(StringView payload)
{
    if (child_options == nullptr)
        throw std::runtime_error("misplaced CGROUP_SET packet");

    auto set = ParseCgroupSet(payload);
    if (set.first.IsNull())
        throw std::runtime_error("malformed CGROUP_SET packet");

    child_options->cgroup.Set(alloc, set.first, set.second);
}

static bool
CheckProbeSuffix(StringView payload) noexcept
{
    return payload.Find('/') == nullptr &&
        !HasNullByte(payload.ToVoid());
}

#if TRANSLATION_ENABLE_TRANSFORMATION

inline void
TranslateParser::HandleSubstYamlFile(StringView payload)
{
    const auto prefix = payload.data;

    auto separator = payload.Find('\0');
    if (separator == nullptr)
        throw std::runtime_error("malformed SUBST_YAML_FILE packet");

    const auto yaml_file = separator + 1;
    if (!IsValidAbsolutePath(yaml_file))
        throw std::runtime_error("malformed SUBST_YAML_FILE packet");

    payload.MoveFront(yaml_file);
    separator = payload.Find('\0');
    if (separator == nullptr)
        throw std::runtime_error("malformed SUBST_YAML_FILE packet");

    const auto yaml_map_path = separator + 1;

    payload.MoveFront(yaml_map_path);
    separator = payload.Find('\0');
    if (separator != nullptr)
        throw std::runtime_error("malformed SUBST_YAML_FILE packet");

    AddSubstYamlFile(prefix, yaml_file, yaml_map_path);
}

#endif

inline void
TranslateParser::HandleRegularPacket(TranslationCommand command,
                                     const ConstBuffer<void> payload)
{
    const StringView string_payload(payload);

    switch (command) {
#if TRANSLATION_ENABLE_TRANSFORMATION
        Transformation *new_transformation;
#endif

    case TranslationCommand::BEGIN:
    case TranslationCommand::END:
        gcc_unreachable();

    case TranslationCommand::PARAM:
    case TranslationCommand::REMOTE_HOST:
    case TranslationCommand::WIDGET_TYPE:
    case TranslationCommand::USER_AGENT:
    case TranslationCommand::ARGS:
    case TranslationCommand::QUERY_STRING:
    case TranslationCommand::LOCAL_ADDRESS:
    case TranslationCommand::LOCAL_ADDRESS_STRING:
    case TranslationCommand::AUTHORIZATION:
    case TranslationCommand::UA_CLASS:
    case TranslationCommand::SUFFIX:
    case TranslationCommand::LISTENER_TAG:
    case TranslationCommand::LOGIN:
    case TranslationCommand::CRON:
    case TranslationCommand::PASSWORD:
    case TranslationCommand::SERVICE:
    case TranslationCommand::ALT_HOST:
        throw std::runtime_error("misplaced translate request packet");

    case TranslationCommand::UID_GID:
        HandleUidGid(payload);
        return;

    case TranslationCommand::STATUS:
        if (payload.size != 2)
            throw std::runtime_error("size mismatch in STATUS packet from translation server");

#if TRANSLATION_ENABLE_HTTP
        response.status = http_status_t(*(const uint16_t*)payload.data);

        if (!http_status_is_valid(response.status))
            throw FormatRuntimeError("invalid HTTP status code %u",
                                     response.status);
#else
        response.status = *(const uint16_t *)payload.data;
#endif

        return;

    case TranslationCommand::PATH:
#if TRANSLATION_ENABLE_RADDRESS
        if (!IsValidAbsolutePath(string_payload))
            throw std::runtime_error("malformed PATH packet");

        if (nfs_address != nullptr && *nfs_address->path == 0) {
            nfs_address->path = string_payload.data;
            return;
        }

        if (resource_address == nullptr || resource_address->IsDefined())
            throw std::runtime_error("misplaced PATH packet");

        file_address = alloc.New<FileAddress>(string_payload.data);
        *resource_address = *file_address;
        return;
#else
        break;
#endif

    case TranslationCommand::PATH_INFO:
#if TRANSLATION_ENABLE_RADDRESS
        if (HasNullByte(payload))
            throw std::runtime_error("malformed PATH_INFO packet");

        if (cgi_address != nullptr &&
            cgi_address->path_info == nullptr) {
            cgi_address->path_info = string_payload.data;
            return;
        } else if (file_address != nullptr) {
            /* don't emit an error when the resource is a local path.
               This combination might be useful one day, but isn't
               currently used. */
            return;
        } else
            throw std::runtime_error("misplaced PATH_INFO packet");
#else
        break;
#endif

    case TranslationCommand::EXPAND_PATH:
#if TRANSLATION_ENABLE_RADDRESS && TRANSLATION_ENABLE_EXPAND
        if (HasNullByte(payload))
            throw std::runtime_error("malformed EXPAND_PATH packet");

        if (response.regex == nullptr) {
            throw std::runtime_error("misplaced EXPAND_PATH packet");
        } else if (cgi_address != nullptr && !cgi_address->expand_path) {
            cgi_address->path = string_payload.data;
            cgi_address->expand_path = true;
            return;
        } else if (nfs_address != nullptr && !nfs_address->expand_path) {
            nfs_address->path = string_payload.data;
            nfs_address->expand_path = true;
            return;
        } else if (file_address != nullptr && !file_address->expand_path) {
            file_address->path = string_payload.data;
            file_address->expand_path = true;
            return;
        } else if (http_address != NULL && !http_address->expand_path) {
            http_address->path = string_payload.data;
            http_address->expand_path = true;
            return;
        } else
            throw std::runtime_error("misplaced EXPAND_PATH packet");
#else
        break;
#endif

    case TranslationCommand::EXPAND_PATH_INFO:
#if TRANSLATION_ENABLE_RADDRESS && TRANSLATION_ENABLE_EXPAND
        if (HasNullByte(payload))
            throw std::runtime_error("malformed EXPAND_PATH_INFO packet");

        if (response.regex == nullptr) {
            throw std::runtime_error("misplaced EXPAND_PATH_INFO packet");
        } else if (cgi_address != nullptr &&
                   !cgi_address->expand_path_info) {
            cgi_address->path_info = string_payload.data;
            cgi_address->expand_path_info = true;
        } else if (file_address != nullptr) {
            /* don't emit an error when the resource is a local path.
               This combination might be useful one day, but isn't
               currently used. */
        } else
            throw std::runtime_error("misplaced EXPAND_PATH_INFO packet");

        return;
#else
        break;
#endif

    case TranslationCommand::DEFLATED:
#if TRANSLATION_ENABLE_RADDRESS
        if (!IsValidAbsolutePath(string_payload))
            throw std::runtime_error("malformed DEFLATED packet");

        if (file_address != nullptr) {
            file_address->deflated = string_payload.data;
            return;
        } else if (nfs_address != nullptr) {
            /* ignore for now */
        } else
            throw std::runtime_error("misplaced DEFLATED packet");
        return;
#else
        break;
#endif

    case TranslationCommand::GZIPPED:
#if TRANSLATION_ENABLE_RADDRESS
        if (!IsValidAbsolutePath(string_payload))
            throw std::runtime_error("malformed GZIPPED packet");

        if (file_address != nullptr) {
            if (file_address->auto_gzipped ||
                file_address->gzipped != nullptr)
                throw std::runtime_error("misplaced GZIPPED packet");

            file_address->gzipped = string_payload.data;
            return;
        } else if (nfs_address != nullptr) {
            /* ignore for now */
            return;
        } else {
            throw std::runtime_error("misplaced GZIPPED packet");
        }

        return;
#else
        break;
#endif

    case TranslationCommand::SITE:
#if TRANSLATION_ENABLE_RADDRESS
        assert(resource_address != nullptr);

        if (!IsValidNonEmptyString(string_payload))
            throw std::runtime_error("malformed SITE packet");

        if (resource_address == &response.address)
#endif
            response.site = (const char *)payload.data;
#if TRANSLATION_ENABLE_RADDRESS
        else if (jail != nullptr && jail->enabled)
            jail->site_id = string_payload.data;
        else
            throw std::runtime_error("misplaced SITE packet");
#endif

        return;

    case TranslationCommand::CONTENT_TYPE:
#if TRANSLATION_ENABLE_RADDRESS
        if (!IsValidNonEmptyString(string_payload))
            throw std::runtime_error("malformed CONTENT_TYPE packet");

        if (file_address != nullptr) {
            if (!file_address->content_type_lookup.IsNull())
                throw std::runtime_error("CONTENT_TYPE/CONTENT_TYPE_LOOKUP conflict");

            file_address->content_type = string_payload.data;
        } else if (nfs_address != nullptr) {
            if (!nfs_address->content_type_lookup.IsNull())
                throw std::runtime_error("CONTENT_TYPE/CONTENT_TYPE_LOOKUP conflict");

            nfs_address->content_type = string_payload.data;
        } else if (from_request.content_type_lookup) {
            response.content_type = string_payload.data;
        } else
            throw std::runtime_error("misplaced CONTENT_TYPE packet");

        return;
#else
        break;
#endif

    case TranslationCommand::HTTP:
#if TRANSLATION_ENABLE_RADDRESS
        if (resource_address == nullptr || resource_address->IsDefined())
            throw std::runtime_error("misplaced HTTP packet");

        if (!IsValidNonEmptyString(string_payload))
            throw std::runtime_error("malformed HTTP packet");

        http_address = http_address_parse(alloc, string_payload.data);

        *resource_address = *http_address;

        address_list = &http_address->addresses;
        default_port = http_address->GetDefaultPort();
        return;
#else
        break;
#endif

    case TranslationCommand::REDIRECT:
#if TRANSLATION_ENABLE_HTTP
        if (!IsValidNonEmptyString(string_payload))
            throw std::runtime_error("malformed REDIRECT packet");

        response.redirect = string_payload.data;
        return;
#else
        break;
#endif

    case TranslationCommand::EXPAND_REDIRECT:
#if TRANSLATION_ENABLE_HTTP && TRANSLATION_ENABLE_EXPAND
        if (response.regex == nullptr ||
            response.redirect == nullptr ||
            response.expand_redirect)
            throw std::runtime_error("misplaced EXPAND_REDIRECT packet");

        if (!IsValidNonEmptyString(string_payload))
            throw std::runtime_error("malformed EXPAND_REDIRECT packet");

        response.redirect = string_payload.data;
        response.expand_redirect = true;
        return;
#else
        break;
#endif

    case TranslationCommand::BOUNCE:
#if TRANSLATION_ENABLE_HTTP
        if (!IsValidNonEmptyString(string_payload))
            throw std::runtime_error("malformed BOUNCE packet");

        response.bounce = string_payload.data;
        return;
#else
        break;
#endif

    case TranslationCommand::FILTER:
#if TRANSLATION_ENABLE_TRANSFORMATION
        resource_address = AddFilter();
        jail = nullptr;
        child_options = nullptr;
        ns_options = nullptr;
        mount_list = nullptr;
        file_address = nullptr;
        cgi_address = nullptr;
        nfs_address = nullptr;
        lhttp_address = nullptr;
        address_list = nullptr;
        return;
#else
        break;
#endif

    case TranslationCommand::FILTER_4XX:
#if TRANSLATION_ENABLE_TRANSFORMATION
        if (view != nullptr)
            view->filter_4xx = true;
        else
            response.filter_4xx = true;
        return;
#else
        break;
#endif

    case TranslationCommand::PROCESS:
#if TRANSLATION_ENABLE_TRANSFORMATION
        new_transformation = AddTransformation(Transformation::Type::PROCESS);
        new_transformation->u.processor.options = PROCESSOR_REWRITE_URL;
        return;
#else
        break;
#endif

    case TranslationCommand::DOMAIN_:
        throw std::runtime_error("deprecated DOMAIN packet");

    case TranslationCommand::CONTAINER:
#if TRANSLATION_ENABLE_TRANSFORMATION
        if (transformation == nullptr ||
            transformation->type != Transformation::Type::PROCESS)
            throw std::runtime_error("misplaced CONTAINER packet");

        transformation->u.processor.options |= PROCESSOR_CONTAINER;
        return;
#else
        break;
#endif

    case TranslationCommand::SELF_CONTAINER:
#if TRANSLATION_ENABLE_TRANSFORMATION
        if (transformation == nullptr ||
            transformation->type != Transformation::Type::PROCESS)
            throw std::runtime_error("misplaced SELF_CONTAINER packet");

        transformation->u.processor.options |=
            PROCESSOR_SELF_CONTAINER|PROCESSOR_CONTAINER;
        return;
#else
        break;
#endif

    case TranslationCommand::GROUP_CONTAINER:
#if TRANSLATION_ENABLE_TRANSFORMATION
        if (!IsValidNonEmptyString(string_payload))
            throw std::runtime_error("malformed GROUP_CONTAINER packet");

        if (transformation == nullptr ||
            transformation->type != Transformation::Type::PROCESS)
            throw std::runtime_error("misplaced GROUP_CONTAINER packet");

        transformation->u.processor.options |= PROCESSOR_CONTAINER;
        response.container_groups.Add(alloc, string_payload.data);
        return;
#else
        break;
#endif

    case TranslationCommand::WIDGET_GROUP:
#if TRANSLATION_ENABLE_WIDGET
        if (!IsValidNonEmptyString(string_payload))
            throw std::runtime_error("malformed WIDGET_GROUP packet");

        response.widget_group = string_payload.data;
        return;
#else
        break;
#endif

    case TranslationCommand::UNTRUSTED:
#if TRANSLATION_ENABLE_WIDGET
        if (!IsValidNonEmptyString(string_payload) ||
            string_payload.front() == '.' ||
            string_payload.back() == '.')
            throw std::runtime_error("malformed UNTRUSTED packet");

        if (response.HasUntrusted())
            throw std::runtime_error("misplaced UNTRUSTED packet");

        response.untrusted = string_payload.data;
        return;
#else
        break;
#endif

    case TranslationCommand::UNTRUSTED_PREFIX:
#if TRANSLATION_ENABLE_HTTP
        if (!IsValidNonEmptyString(string_payload) ||
            string_payload.front() == '.' ||
            string_payload.back() == '.')
            throw std::runtime_error("malformed UNTRUSTED_PREFIX packet");

        if (response.HasUntrusted())
            throw std::runtime_error("misplaced UNTRUSTED_PREFIX packet");

        response.untrusted_prefix = string_payload.data;
        return;
#else
        break;
#endif

    case TranslationCommand::UNTRUSTED_SITE_SUFFIX:
#if TRANSLATION_ENABLE_HTTP
        if (!IsValidNonEmptyString(string_payload) ||
            string_payload.front() == '.' ||
            string_payload.back() == '.')
            throw std::runtime_error("malformed UNTRUSTED_SITE_SUFFIX packet");

        if (response.HasUntrusted())
            throw std::runtime_error("misplaced UNTRUSTED_SITE_SUFFIX packet");

        response.untrusted_site_suffix = string_payload.data;
        return;
#else
        break;
#endif

    case TranslationCommand::SCHEME:
#if TRANSLATION_ENABLE_HTTP
        if (!string_payload.StartsWith("http"))
            throw std::runtime_error("malformed SCHEME packet");

        response.scheme = string_payload.data;
        return;
#else
        break;
#endif

    case TranslationCommand::HOST:
#if TRANSLATION_ENABLE_HTTP
        response.host = string_payload.data;
        return;
#else
        break;
#endif

    case TranslationCommand::URI:
#if TRANSLATION_ENABLE_HTTP
        if (!IsValidAbsoluteUriPath(string_payload))
            throw std::runtime_error("malformed URI packet");

        response.uri = string_payload.data;
        return;
#else
        break;
#endif

    case TranslationCommand::DIRECT_ADDRESSING:
#if TRANSLATION_ENABLE_WIDGET
        response.direct_addressing = true;
#endif
        return;

    case TranslationCommand::STATEFUL:
#if TRANSLATION_ENABLE_SESSION
        response.stateful = true;
        return;
#else
        break;
#endif

    case TranslationCommand::SESSION:
#if TRANSLATION_ENABLE_SESSION
        response.session = payload;
        return;
#else
        break;
#endif

    case TranslationCommand::USER:
#if TRANSLATION_ENABLE_SESSION
        response.user = string_payload.data;
        previous_command = command;
        return;
#else
        break;
#endif

    case TranslationCommand::REALM:
#if TRANSLATION_ENABLE_SESSION
        if (!payload.empty())
            throw std::runtime_error("malformed REALM packet");

        if (response.realm != nullptr)
            throw std::runtime_error("duplicate REALM packet");

        if (response.realm_from_auth_base)
            throw std::runtime_error("misplaced REALM packet");

        response.realm = string_payload.data;
        return;
#else
        break;
#endif

    case TranslationCommand::LANGUAGE:
#if TRANSLATION_ENABLE_SESSION
        response.language = string_payload.data;
        return;
#else
        break;
#endif

    case TranslationCommand::PIPE:
#if TRANSLATION_ENABLE_RADDRESS
        if (resource_address == nullptr || resource_address->IsDefined())
            throw std::runtime_error("misplaced PIPE packet");

        if (payload.empty())
            throw std::runtime_error("malformed PIPE packet");

        SetCgiAddress(ResourceAddress::Type::PIPE, string_payload.data);
        return;
#else
        break;
#endif

    case TranslationCommand::CGI:
#if TRANSLATION_ENABLE_RADDRESS
        if (resource_address == nullptr || resource_address->IsDefined())
            throw std::runtime_error("misplaced CGI packet");

        if (!IsValidAbsolutePath(string_payload))
            throw std::runtime_error("malformed CGI packet");

        SetCgiAddress(ResourceAddress::Type::CGI, string_payload.data);
        cgi_address->document_root = response.document_root;
        return;
#else
        break;
#endif

    case TranslationCommand::FASTCGI:
#if TRANSLATION_ENABLE_RADDRESS
        if (resource_address == nullptr || resource_address->IsDefined())
            throw std::runtime_error("misplaced FASTCGI packet");

        if (!IsValidAbsolutePath(string_payload))
            throw std::runtime_error("malformed FASTCGI packet");

        SetCgiAddress(ResourceAddress::Type::FASTCGI, string_payload.data);
        address_list = &cgi_address->address_list;
        default_port = 9000;
        return;
#else
        break;
#endif

    case TranslationCommand::AJP:
#if TRANSLATION_ENABLE_RADDRESS
        throw std::runtime_error("AJP support has been removed");
#else
        break;
#endif

    case TranslationCommand::NFS_SERVER:
#if TRANSLATION_ENABLE_RADDRESS
        if (resource_address == nullptr || resource_address->IsDefined())
            throw std::runtime_error("misplaced NFS_SERVER packet");

        if (payload.empty())
            throw std::runtime_error("malformed NFS_SERVER packet");

        nfs_address = alloc.New<NfsAddress>(string_payload.data, "", "");
        *resource_address = *nfs_address;
        return;
#else
        break;
#endif

    case TranslationCommand::NFS_EXPORT:
#if TRANSLATION_ENABLE_RADDRESS
        if (nfs_address == nullptr ||
            *nfs_address->export_name != 0)
            throw std::runtime_error("misplaced NFS_EXPORT packet");

        if (!IsValidAbsolutePath(string_payload))
            throw std::runtime_error("malformed NFS_EXPORT packet");

        nfs_address->export_name = string_payload.data;
        return;
#else
        break;
#endif

    case TranslationCommand::JAILCGI:
#if TRANSLATION_ENABLE_JAILCGI
        if (jail == nullptr) {
            if (child_options == nullptr)
                throw std::runtime_error("misplaced JAILCGI packet");

            jail = child_options->jail = alloc.New<JailParams>();
        }

        jail->enabled = true;
        return;
#endif

    case TranslationCommand::HOME:
        translate_client_home(ns_options,
#if TRANSLATION_ENABLE_JAILCGI
                              jail,
#endif
                              string_payload);
        return;

    case TranslationCommand::INTERPRETER:
#if TRANSLATION_ENABLE_RADDRESS
        if (resource_address == nullptr ||
            (resource_address->type != ResourceAddress::Type::CGI &&
             resource_address->type != ResourceAddress::Type::FASTCGI) ||
            cgi_address->interpreter != nullptr)
            throw std::runtime_error("misplaced INTERPRETER packet");

        cgi_address->interpreter = string_payload.data;
        return;
#else
        break;
#endif

    case TranslationCommand::ACTION:
#if TRANSLATION_ENABLE_RADDRESS
        if (resource_address == nullptr ||
            (resource_address->type != ResourceAddress::Type::CGI &&
             resource_address->type != ResourceAddress::Type::FASTCGI) ||
            cgi_address->action != nullptr)
            throw std::runtime_error("misplaced ACTION packet");

        cgi_address->action = string_payload.data;
        return;
#else
        break;
#endif

    case TranslationCommand::SCRIPT_NAME:
#if TRANSLATION_ENABLE_RADDRESS
        if (resource_address == nullptr ||
            (resource_address->type != ResourceAddress::Type::CGI &&
             resource_address->type != ResourceAddress::Type::WAS &&
             resource_address->type != ResourceAddress::Type::FASTCGI) ||
            cgi_address->script_name != nullptr)
            throw std::runtime_error("misplaced SCRIPT_NAME packet");

        cgi_address->script_name = string_payload.data;
        return;
#else
        break;
#endif

    case TranslationCommand::EXPAND_SCRIPT_NAME:
#if TRANSLATION_ENABLE_RADDRESS && TRANSLATION_ENABLE_EXPAND
        if (!IsValidNonEmptyString(string_payload))
            throw std::runtime_error("malformed EXPAND_SCRIPT_NAME packet");

        if (response.regex == nullptr ||
            cgi_address == nullptr ||
            cgi_address->expand_script_name)
            throw std::runtime_error("misplaced EXPAND_SCRIPT_NAME packet");

        cgi_address->script_name = string_payload.data;
        cgi_address->expand_script_name = true;
        return;
#else
        break;
#endif

    case TranslationCommand::DOCUMENT_ROOT:
#if TRANSLATION_ENABLE_RADDRESS
        if (!IsValidAbsolutePath(string_payload))
            throw std::runtime_error("malformed DOCUMENT_ROOT packet");

        if (cgi_address != nullptr)
            cgi_address->document_root = string_payload.data;
        else if (file_address != nullptr &&
                 file_address->delegate != nullptr)
            file_address->document_root = string_payload.data;
        else
            response.document_root = string_payload.data;
        return;
#else
        break;
#endif

    case TranslationCommand::EXPAND_DOCUMENT_ROOT:
#if TRANSLATION_ENABLE_RADDRESS && TRANSLATION_ENABLE_EXPAND
        if (!IsValidNonEmptyString(string_payload))
            throw std::runtime_error("malformed EXPAND_DOCUMENT_ROOT packet");

        if (response.regex == nullptr)
            throw std::runtime_error("misplaced EXPAND_DOCUMENT_ROOT packet");

        if (cgi_address != nullptr) {
            cgi_address->document_root = string_payload.data;
            cgi_address->expand_document_root = true;
        } else if (file_address != nullptr &&
                   file_address->delegate != nullptr) {
            file_address->document_root = string_payload.data;
            file_address->expand_document_root = true;
        } else {
            response.document_root = string_payload.data;
            response.expand_document_root = true;
        }
        return;
#else
        break;
#endif

    case TranslationCommand::ADDRESS:
#if TRANSLATION_ENABLE_HTTP
        if (address_list == nullptr)
            throw std::runtime_error("misplaced ADDRESS packet");

        if (payload.size < 2)
            throw std::runtime_error("malformed ADDRESS packet");

        address_list->Add(alloc,
                          SocketAddress((const struct sockaddr *)payload.data,
                                        payload.size));
        return;
#else
        break;
#endif

    case TranslationCommand::ADDRESS_STRING:
#if TRANSLATION_ENABLE_HTTP
        if (address_list == nullptr)
            throw std::runtime_error("misplaced ADDRESS_STRING packet");

        if (!IsValidNonEmptyString(string_payload))
            throw std::runtime_error("malformed ADDRESS_STRING packet");

        try {
            parse_address_string(alloc, address_list,
                                 string_payload.data, default_port);
        } catch (const std::exception &e) {
            throw FormatRuntimeError("malformed ADDRESS_STRING packet: %s",
                                     e.what());
        }

        return;
#else
        break;
#endif

    case TranslationCommand::VIEW:
#if TRANSLATION_ENABLE_WIDGET
        if (!valid_view_name(string_payload.data))
            throw std::runtime_error("invalid view name");

        AddView(string_payload.data);
        return;
#else
        break;
#endif

    case TranslationCommand::MAX_AGE:
        if (payload.size != 4)
            throw std::runtime_error("malformed MAX_AGE packet");

        switch (previous_command) {
        case TranslationCommand::BEGIN:
            response.max_age = std::chrono::seconds(*(const uint32_t *)payload.data);
            break;

#if TRANSLATION_ENABLE_SESSION
        case TranslationCommand::USER:
            response.user_max_age = std::chrono::seconds(*(const uint32_t *)payload.data);
            break;
#endif

        default:
            throw std::runtime_error("misplaced MAX_AGE packet");
        }

        return;

    case TranslationCommand::VARY:
#if TRANSLATION_ENABLE_CACHE
        if (payload.empty() ||
            payload.size % sizeof(response.vary.data[0]) != 0)
            throw std::runtime_error("malformed VARY packet");

        response.vary.data = (const TranslationCommand *)payload.data;
        response.vary.size = payload.size / sizeof(response.vary.data[0]);
#endif
        return;

    case TranslationCommand::INVALIDATE:
#if TRANSLATION_ENABLE_CACHE
        if (payload.empty() ||
            payload.size % sizeof(response.invalidate.data[0]) != 0)
            throw std::runtime_error("malformed INVALIDATE packet");

        response.invalidate.data = (const TranslationCommand *)payload.data;
        response.invalidate.size = payload.size /
            sizeof(response.invalidate.data[0]);
#endif
        return;

    case TranslationCommand::BASE:
#if TRANSLATION_ENABLE_RADDRESS
        if (!IsValidAbsoluteUriPath(string_payload) ||
            string_payload.back() != '/')
            throw std::runtime_error("malformed BASE packet");

        if (from_request.uri == nullptr ||
            response.auto_base ||
            response.base != nullptr)
            throw std::runtime_error("misplaced BASE packet");

        if (!StringStartsWith(from_request.uri, string_payload))
            throw std::runtime_error("BASE mismatches request URI");

        response.base = string_payload.data;
        return;
#else
        break;
#endif

    case TranslationCommand::UNSAFE_BASE:
#if TRANSLATION_ENABLE_RADDRESS
        if (!payload.empty())
            throw std::runtime_error("malformed UNSAFE_BASE packet");

        if (response.base == nullptr)
            throw std::runtime_error("misplaced UNSAFE_BASE packet");

        response.unsafe_base = true;
        return;
#else
        break;
#endif

    case TranslationCommand::EASY_BASE:
#if TRANSLATION_ENABLE_RADDRESS
        if (!payload.empty())
            throw std::runtime_error("malformed EASY_BASE");

        if (response.base == nullptr)
            throw std::runtime_error("EASY_BASE without BASE");

        if (response.easy_base)
            throw std::runtime_error("duplicate EASY_BASE");

        response.easy_base = true;
        return;
#else
        break;
#endif

    case TranslationCommand::REGEX:
#if TRANSLATION_ENABLE_EXPAND
        if (response.base == nullptr)
            throw std::runtime_error("REGEX without BASE");

        if (response.regex != nullptr)
            throw std::runtime_error("duplicate REGEX");

        if (!IsValidNonEmptyString(string_payload))
            throw std::runtime_error("malformed REGEX packet");

        response.regex = string_payload.data;
        return;
#else
        break;
#endif

    case TranslationCommand::INVERSE_REGEX:
#if TRANSLATION_ENABLE_EXPAND
        if (response.base == nullptr)
            throw std::runtime_error("INVERSE_REGEX without BASE");

        if (response.inverse_regex != nullptr)
            throw std::runtime_error("duplicate INVERSE_REGEX");

        if (!IsValidNonEmptyString(string_payload))
            throw std::runtime_error("malformed INVERSE_REGEX packet");

        response.inverse_regex = string_payload.data;
        return;
#else
        break;
#endif

    case TranslationCommand::REGEX_TAIL:
#if TRANSLATION_ENABLE_EXPAND
        if (!payload.empty())
            throw std::runtime_error("malformed REGEX_TAIL packet");

        if (response.regex == nullptr && response.inverse_regex == nullptr)
            throw std::runtime_error("misplaced REGEX_TAIL packet");

        if (response.regex_tail)
            throw std::runtime_error("duplicate REGEX_TAIL packet");

        response.regex_tail = true;
        return;
#else
        break;
#endif

    case TranslationCommand::REGEX_UNESCAPE:
#if TRANSLATION_ENABLE_EXPAND
        if (!payload.empty())
            throw std::runtime_error("malformed REGEX_UNESCAPE packet");

        if (response.regex == nullptr && response.inverse_regex == nullptr)
            throw std::runtime_error("misplaced REGEX_UNESCAPE packet");

        if (response.regex_unescape)
            throw std::runtime_error("duplicate REGEX_UNESCAPE packet");

        response.regex_unescape = true;
        return;
#else
        break;
#endif

    case TranslationCommand::DELEGATE:
#if TRANSLATION_ENABLE_RADDRESS
        if (file_address == nullptr)
            throw std::runtime_error("misplaced DELEGATE packet");

        if (!IsValidAbsolutePath(string_payload))
            throw std::runtime_error("malformed DELEGATE packet");

        file_address->delegate = alloc.New<DelegateAddress>(string_payload.data);
        SetChildOptions(file_address->delegate->child_options);
        return;
#else
        break;
#endif

    case TranslationCommand::APPEND:
        if (!IsValidNonEmptyString(string_payload))
            throw std::runtime_error("malformed APPEND packet");

        if (!HasArgs())
            throw std::runtime_error("misplaced APPEND packet");

        args_builder.Add(alloc, (const char *)payload.data, false);
        return;

    case TranslationCommand::EXPAND_APPEND:
#if TRANSLATION_ENABLE_EXPAND
        if (!IsValidNonEmptyString(string_payload))
            throw std::runtime_error("malformed EXPAND_APPEND packet");

        if (response.regex == nullptr || !HasArgs() ||
            !args_builder.CanSetExpand())
            throw std::runtime_error("misplaced EXPAND_APPEND packet");

        args_builder.SetExpand(string_payload.data);
        return;
#else
        break;
#endif

    case TranslationCommand::PAIR:
#if TRANSLATION_ENABLE_RADDRESS
        if (cgi_address != nullptr &&
            resource_address->type != ResourceAddress::Type::CGI &&
            resource_address->type != ResourceAddress::Type::PIPE) {
            translate_client_pair(alloc, params_builder, "PAIR",
                                  string_payload);
            return;
        }
#endif

        if (child_options != nullptr) {
            translate_client_pair(alloc, env_builder, "PAIR",
                                  string_payload);
        } else
            throw std::runtime_error("misplaced PAIR packet");
        return;

    case TranslationCommand::EXPAND_PAIR:
#if TRANSLATION_ENABLE_RADDRESS
        if (response.regex == nullptr)
            throw std::runtime_error("misplaced EXPAND_PAIR packet");

        if (cgi_address != nullptr) {
            const auto type = resource_address->type;
            auto &builder = type == ResourceAddress::Type::CGI
                ? env_builder
                : params_builder;

            translate_client_expand_pair(builder, "EXPAND_PAIR",
                                         string_payload);
        } else if (lhttp_address != nullptr) {
            translate_client_expand_pair(env_builder,
                                         "EXPAND_PAIR",
                                         string_payload);
        } else
            throw std::runtime_error("misplaced EXPAND_PAIR packet");
        return;
#else
        break;
#endif

    case TranslationCommand::DISCARD_SESSION:
#if TRANSLATION_ENABLE_SESSION
        response.discard_session = true;
        return;
#else
        break;
#endif

    case TranslationCommand::REQUEST_HEADER_FORWARD:
#if TRANSLATION_ENABLE_HTTP
        if (view != nullptr)
            parse_header_forward(&view->request_header_forward,
                                 payload);
        else
            parse_header_forward(&response.request_header_forward,
                                 payload);
        return;
#else
        break;
#endif

    case TranslationCommand::RESPONSE_HEADER_FORWARD:
#if TRANSLATION_ENABLE_HTTP
        if (view != nullptr)
            parse_header_forward(&view->response_header_forward,
                                 payload);
        else
            parse_header_forward(&response.response_header_forward,
                                 payload);
        return;
#else
        break;
#endif

    case TranslationCommand::WWW_AUTHENTICATE:
#if TRANSLATION_ENABLE_SESSION
        if (!IsValidNonEmptyString(string_payload))
            throw std::runtime_error("malformed WWW_AUTHENTICATE packet");

        response.www_authenticate = string_payload.data;
        return;
#else
        break;
#endif

    case TranslationCommand::AUTHENTICATION_INFO:
#if TRANSLATION_ENABLE_SESSION
        if (!IsValidNonEmptyString(string_payload))
            throw std::runtime_error("malformed AUTHENTICATION_INFO packet");

        response.authentication_info = string_payload.data;
        return;
#else
        break;
#endif

    case TranslationCommand::HEADER:
#if TRANSLATION_ENABLE_HTTP
        parse_header(alloc, response.response_headers,
                     "HEADER", string_payload);
        return;
#else
        break;
#endif

    case TranslationCommand::SECURE_COOKIE:
#if TRANSLATION_ENABLE_SESSION
        response.secure_cookie = true;
        return;
#else
        break;
#endif

    case TranslationCommand::COOKIE_DOMAIN:
#if TRANSLATION_ENABLE_SESSION
        if (response.cookie_domain != nullptr)
            throw std::runtime_error("misplaced COOKIE_DOMAIN packet");

        if (!IsValidNonEmptyString(string_payload))
            throw std::runtime_error("malformed COOKIE_DOMAIN packet");

        response.cookie_domain = string_payload.data;
        return;
#else
        break;
#endif

    case TranslationCommand::ERROR_DOCUMENT:
        response.error_document = payload;
        return;

    case TranslationCommand::CHECK:
#if TRANSLATION_ENABLE_SESSION
        if (!response.check.IsNull())
            throw std::runtime_error("duplicate CHECK packet");

        response.check = payload;
        return;
#else
        break;
#endif

    case TranslationCommand::PREVIOUS:
        response.previous = true;
        return;

    case TranslationCommand::WAS:
#if TRANSLATION_ENABLE_RADDRESS
        if (resource_address == nullptr || resource_address->IsDefined())
            throw std::runtime_error("misplaced WAS packet");

        if (!IsValidAbsolutePath(string_payload))
            throw std::runtime_error("malformed WAS packet");

        SetCgiAddress(ResourceAddress::Type::WAS, string_payload.data);
        return;
#else
        break;
#endif

    case TranslationCommand::TRANSPARENT:
        response.transparent = true;
        return;

    case TranslationCommand::WIDGET_INFO:
#if TRANSLATION_ENABLE_WIDGET
        response.widget_info = true;
#endif
        return;

    case TranslationCommand::STICKY:
#if TRANSLATION_ENABLE_RADDRESS
        if (address_list == nullptr)
            throw std::runtime_error("misplaced STICKY packet");

        address_list->SetStickyMode(StickyMode::SESSION_MODULO);
        return;
#else
        break;
#endif

    case TranslationCommand::DUMP_HEADERS:
#if TRANSLATION_ENABLE_HTTP
        response.dump_headers = true;
#endif
        return;

    case TranslationCommand::COOKIE_HOST:
#if TRANSLATION_ENABLE_SESSION
        if (resource_address == nullptr || !resource_address->IsDefined())
            throw std::runtime_error("misplaced COOKIE_HOST packet");

        if (!IsValidNonEmptyString(string_payload))
            throw std::runtime_error("malformed COOKIE_HOST packet");

        response.cookie_host = string_payload.data;
        return;
#else
        break;
#endif

    case TranslationCommand::COOKIE_PATH:
#if TRANSLATION_ENABLE_SESSION
        if (response.cookie_path != nullptr)
            throw std::runtime_error("misplaced COOKIE_PATH packet");

        if (!IsValidAbsoluteUriPath(string_payload))
            throw std::runtime_error("malformed COOKIE_PATH packet");

        response.cookie_path = string_payload.data;
        return;
#else
        break;
#endif

    case TranslationCommand::PROCESS_CSS:
#if TRANSLATION_ENABLE_TRANSFORMATION
        new_transformation = AddTransformation(Transformation::Type::PROCESS_CSS);
        new_transformation->u.css_processor.options = CSS_PROCESSOR_REWRITE_URL;
        return;
#else
        break;
#endif

    case TranslationCommand::PREFIX_CSS_CLASS:
#if TRANSLATION_ENABLE_TRANSFORMATION
        if (transformation == nullptr)
            throw std::runtime_error("misplaced PREFIX_CSS_CLASS packet");

        switch (transformation->type) {
        case Transformation::Type::PROCESS:
            transformation->u.processor.options |= PROCESSOR_PREFIX_CSS_CLASS;
            break;

        case Transformation::Type::PROCESS_CSS:
            transformation->u.css_processor.options |= CSS_PROCESSOR_PREFIX_CLASS;
            break;

        default:
            throw std::runtime_error("misplaced PREFIX_CSS_CLASS packet");
        }

        return;
#else
        break;
#endif

    case TranslationCommand::PREFIX_XML_ID:
#if TRANSLATION_ENABLE_TRANSFORMATION
        if (transformation == nullptr)
            throw std::runtime_error("misplaced PREFIX_XML_ID packet");

        switch (transformation->type) {
        case Transformation::Type::PROCESS:
            transformation->u.processor.options |= PROCESSOR_PREFIX_XML_ID;
            break;

        case Transformation::Type::PROCESS_CSS:
            transformation->u.css_processor.options |= CSS_PROCESSOR_PREFIX_ID;
            break;

        default:
            throw std::runtime_error("misplaced PREFIX_XML_ID packet");
        }

        return;
#else
        break;
#endif

    case TranslationCommand::PROCESS_STYLE:
#if TRANSLATION_ENABLE_TRANSFORMATION
        if (transformation == nullptr ||
            transformation->type != Transformation::Type::PROCESS)
            throw std::runtime_error("misplaced PROCESS_STYLE packet");

        transformation->u.processor.options |= PROCESSOR_STYLE;
        return;
#else
        break;
#endif

    case TranslationCommand::FOCUS_WIDGET:
#if TRANSLATION_ENABLE_TRANSFORMATION
        if (transformation == nullptr ||
            transformation->type != Transformation::Type::PROCESS)
            throw std::runtime_error("misplaced FOCUS_WIDGET packet");

        transformation->u.processor.options |= PROCESSOR_FOCUS_WIDGET;
        return;
#else
        break;
#endif

    case TranslationCommand::ANCHOR_ABSOLUTE:
#if TRANSLATION_ENABLE_WIDGET
        if (transformation == nullptr ||
            transformation->type != Transformation::Type::PROCESS)
            throw std::runtime_error("misplaced ANCHOR_ABSOLUTE packet");

        response.anchor_absolute = true;
        return;
#else
        break;
#endif

    case TranslationCommand::PROCESS_TEXT:
#if TRANSLATION_ENABLE_TRANSFORMATION
        new_transformation = AddTransformation(Transformation::Type::PROCESS_TEXT);
        return;
#else
        break;
#endif

    case TranslationCommand::LOCAL_URI:
#if TRANSLATION_ENABLE_HTTP
        if (response.local_uri != nullptr)
            throw std::runtime_error("misplaced LOCAL_URI packet");

        if (string_payload.empty() || string_payload.back() != '/')
            throw std::runtime_error("malformed LOCAL_URI packet");

        response.local_uri = string_payload.data;
        return;
#else
        break;
#endif

    case TranslationCommand::AUTO_BASE:
#if TRANSLATION_ENABLE_RADDRESS
        if (resource_address != &response.address ||
            cgi_address == nullptr ||
            cgi_address != &response.address.GetCgi() ||
            cgi_address->path_info == nullptr ||
            from_request.uri == nullptr ||
            response.base != nullptr ||
            response.auto_base)
            throw std::runtime_error("misplaced AUTO_BASE packet");

        response.auto_base = true;
        return;
#else
        break;
#endif

    case TranslationCommand::VALIDATE_MTIME:
        if (string_payload.size < 10 || string_payload[8] != '/' ||
            memchr(string_payload.data + 9, 0,
                   string_payload.size - 9) != nullptr)
            throw std::runtime_error("malformed VALIDATE_MTIME packet");

        response.validate_mtime.mtime = *(const uint64_t *)payload.data;
        response.validate_mtime.path =
            alloc.DupZ({string_payload.data + 8, string_payload.size - 8});
        return;

    case TranslationCommand::LHTTP_PATH:
#if TRANSLATION_ENABLE_RADDRESS
        if (resource_address == nullptr || resource_address->IsDefined())
            throw std::runtime_error("misplaced LHTTP_PATH packet");

        if (!IsValidAbsolutePath(string_payload))
            throw std::runtime_error("malformed LHTTP_PATH packet");

        lhttp_address = alloc.New<LhttpAddress>(string_payload.data);
        *resource_address = *lhttp_address;

        args_builder = lhttp_address->args;
        SetChildOptions(lhttp_address->options);
        return;
#else
        break;
#endif

    case TranslationCommand::LHTTP_URI:
#if TRANSLATION_ENABLE_RADDRESS
        if (lhttp_address == nullptr ||
            lhttp_address->uri != nullptr)
            throw std::runtime_error("misplaced LHTTP_HOST packet");

        if (!IsValidAbsoluteUriPath(string_payload))
            throw std::runtime_error("malformed LHTTP_URI packet");

        lhttp_address->uri = string_payload.data;
        return;
#else
        break;
#endif

    case TranslationCommand::EXPAND_LHTTP_URI:
#if TRANSLATION_ENABLE_RADDRESS
        if (lhttp_address == nullptr ||
            lhttp_address->expand_uri ||
            response.regex == nullptr)
            throw std::runtime_error("misplaced EXPAND_LHTTP_URI packet");

        if (!IsValidNonEmptyString(string_payload))
            throw std::runtime_error("malformed EXPAND_LHTTP_URI packet");

        lhttp_address->uri = string_payload.data;
        lhttp_address->expand_uri = true;
        return;
#else
        break;
#endif

    case TranslationCommand::LHTTP_HOST:
#if TRANSLATION_ENABLE_RADDRESS
        if (lhttp_address == nullptr ||
            lhttp_address->host_and_port != nullptr)
            throw std::runtime_error("misplaced LHTTP_HOST packet");

        if (!IsValidNonEmptyString(string_payload))
            throw std::runtime_error("malformed LHTTP_HOST packet");

        lhttp_address->host_and_port = string_payload.data;
        return;
#else
        break;
#endif

    case TranslationCommand::CONCURRENCY:
#if TRANSLATION_ENABLE_RADDRESS
        if (lhttp_address == nullptr)
            throw std::runtime_error("misplaced CONCURRENCY packet");

        if (payload.size != 2)
            throw std::runtime_error("malformed CONCURRENCY packet");

        lhttp_address->concurrency = *(const uint16_t *)payload.data;
        return;
#else
        break;
#endif

    case TranslationCommand::WANT_FULL_URI:
#if TRANSLATION_ENABLE_HTTP
        if (from_request.want_full_uri)
            throw std::runtime_error("WANT_FULL_URI loop");

        if (!response.want_full_uri.IsNull())
            throw std::runtime_error("duplicate WANT_FULL_URI packet");

        response.want_full_uri = payload;
        return;
#else
        break;
#endif

    case TranslationCommand::USER_NAMESPACE:
        if (!payload.empty())
            throw std::runtime_error("malformed USER_NAMESPACE packet");

        if (ns_options != nullptr) {
            ns_options->enable_user = true;
        } else
            throw std::runtime_error("misplaced USER_NAMESPACE packet");

        return;

    case TranslationCommand::PID_NAMESPACE:
        if (!payload.empty())
            throw std::runtime_error("malformed PID_NAMESPACE packet");

        if (ns_options != nullptr) {
            ns_options->enable_pid = true;
        } else
            throw std::runtime_error("misplaced PID_NAMESPACE packet");

        if (ns_options->pid_namespace != nullptr)
            throw std::runtime_error("Can't combine PID_NAMESPACE with PID_NAMESPACE_NAME");

        return;

    case TranslationCommand::NETWORK_NAMESPACE:
        if (!payload.empty())
            throw std::runtime_error("malformed NETWORK_NAMESPACE packet");

        if (ns_options == nullptr)
            throw std::runtime_error("misplaced NETWORK_NAMESPACE packet");

        if (ns_options->enable_network)
            throw std::runtime_error("duplicate NETWORK_NAMESPACE packet");

        if (ns_options->network_namespace != nullptr)
            throw std::runtime_error("Can't combine NETWORK_NAMESPACE with NETWORK_NAMESPACE_NAME");

        ns_options->enable_network = true;
        return;

    case TranslationCommand::PIVOT_ROOT:
        translate_client_pivot_root(ns_options, string_payload);
        return;

    case TranslationCommand::MOUNT_PROC:
        translate_client_mount_proc(ns_options, payload.size);
        return;

    case TranslationCommand::MOUNT_HOME:
        translate_client_mount_home(ns_options, string_payload);
        return;

    case TranslationCommand::BIND_MOUNT:
        HandleBindMount(string_payload, false, false);
        return;

    case TranslationCommand::MOUNT_TMP_TMPFS:
        translate_client_mount_tmp_tmpfs(ns_options, string_payload);
        return;

    case TranslationCommand::UTS_NAMESPACE:
        translate_client_uts_namespace(ns_options, string_payload);
        return;

    case TranslationCommand::RLIMITS:
        translate_client_rlimits(alloc, child_options, string_payload.data);
        return;

    case TranslationCommand::WANT:
#if TRANSLATION_ENABLE_WANT
        HandleWant((const TranslationCommand *)payload.data, payload.size);
        return;
#else
        break;
#endif

    case TranslationCommand::FILE_NOT_FOUND:
#if TRANSLATION_ENABLE_RADDRESS
        translate_client_file_not_found(response, payload);
        return;
#else
        break;
#endif

    case TranslationCommand::CONTENT_TYPE_LOOKUP:
#if TRANSLATION_ENABLE_RADDRESS
        HandleContentTypeLookup(payload);
        return;
#else
        break;
#endif

    case TranslationCommand::DIRECTORY_INDEX:
#if TRANSLATION_ENABLE_RADDRESS
        translate_client_directory_index(response, payload);
        return;
#else
        break;
#endif

    case TranslationCommand::EXPIRES_RELATIVE:
        translate_client_expires_relative(response, payload);
        return;


    case TranslationCommand::TEST_PATH:
        if (!IsValidAbsolutePath(string_payload))
            throw std::runtime_error("malformed TEST_PATH packet");

        if (response.test_path != nullptr)
            throw std::runtime_error("duplicate TEST_PATH packet");

        response.test_path = string_payload.data;
        return;

    case TranslationCommand::EXPAND_TEST_PATH:
#if TRANSLATION_ENABLE_EXPAND
        if (response.regex == nullptr)
            throw std::runtime_error("misplaced EXPAND_TEST_PATH packet");

        if (!IsValidNonEmptyString(string_payload))
            throw std::runtime_error("malformed EXPAND_TEST_PATH packet");

        if (response.expand_test_path)
            throw std::runtime_error("duplicate EXPAND_TEST_PATH packet");

        response.test_path = string_payload.data;
        response.expand_test_path = true;
        return;
#else
        break;
#endif

    case TranslationCommand::REDIRECT_QUERY_STRING:
#if TRANSLATION_ENABLE_HTTP
        if (!payload.empty())
            throw std::runtime_error("malformed REDIRECT_QUERY_STRING packet");

        if (response.redirect_query_string ||
            response.redirect == nullptr)
            throw std::runtime_error("misplaced REDIRECT_QUERY_STRING packet");

        response.redirect_query_string = true;
        return;
#else
        break;
#endif

    case TranslationCommand::ENOTDIR_:
#if TRANSLATION_ENABLE_RADDRESS
        translate_client_enotdir(response, payload);
        return;
#else
        break;
#endif

    case TranslationCommand::STDERR_PATH:
        translate_client_stderr_path(child_options,
                                     payload,
                                     false);
        return;

    case TranslationCommand::AUTH:
#if TRANSLATION_ENABLE_SESSION
        if (response.HasAuth())
            throw std::runtime_error("duplicate AUTH packet");

        response.auth = payload;
        return;
#else
        break;
#endif

    case TranslationCommand::SETENV:
        if (child_options != nullptr) {
            translate_client_pair(alloc, env_builder,
                                  "SETENV",
                                  string_payload);
        } else
            throw std::runtime_error("misplaced SETENV packet");
        return;

    case TranslationCommand::EXPAND_SETENV:
#if TRANSLATION_ENABLE_EXPAND
        if (response.regex == nullptr)
            throw std::runtime_error("misplaced EXPAND_SETENV packet");

        if (child_options != nullptr) {
            translate_client_expand_pair(env_builder,
                                         "EXPAND_SETENV",
                                         string_payload);
        } else
            throw std::runtime_error("misplaced SETENV packet");
        return;
#else
        break;
#endif

    case TranslationCommand::EXPAND_URI:
#if TRANSLATION_ENABLE_EXPAND
        if (response.regex == nullptr ||
            response.expand_uri)
            throw std::runtime_error("misplaced EXPAND_URI packet");

        if (!IsValidNonEmptyString(string_payload))
            throw std::runtime_error("malformed EXPAND_URI packet");

        response.uri = string_payload.data;
        response.expand_uri = true;
        return;
#else
        break;
#endif

    case TranslationCommand::EXPAND_SITE:
#if TRANSLATION_ENABLE_EXPAND
        if (response.regex == nullptr ||
            response.site == nullptr ||
            response.expand_site)
            throw std::runtime_error("misplaced EXPAND_SITE packet");

        if (!IsValidNonEmptyString(string_payload))
            throw std::runtime_error("malformed EXPAND_SITE packet");

        response.site = string_payload.data;
        response.expand_site = true;
        return;
#endif

    case TranslationCommand::REQUEST_HEADER:
#if TRANSLATION_ENABLE_HTTP
        parse_header(alloc, response.request_headers,
                     "REQUEST_HEADER", string_payload);
        return;
#else
        break;
#endif

    case TranslationCommand::EXPAND_REQUEST_HEADER:
#if TRANSLATION_ENABLE_HTTP && TRANSLATION_ENABLE_EXPAND
        if (response.regex == nullptr)
            throw std::runtime_error("misplaced EXPAND_REQUEST_HEADERS packet");

        parse_header(alloc,
                     response.expand_request_headers,
                     "EXPAND_REQUEST_HEADER", string_payload);
        return;
#else
        break;
#endif

    case TranslationCommand::AUTO_GZIPPED:
#if TRANSLATION_ENABLE_EXPAND
        if (!payload.empty())
            throw std::runtime_error("malformed AUTO_GZIPPED packet");

        if (file_address != nullptr) {
            if (file_address->auto_gzipped ||
                file_address->gzipped != nullptr)
                throw std::runtime_error("misplaced AUTO_GZIPPED packet");

            file_address->auto_gzipped = true;
        } else if (nfs_address != nullptr) {
            /* ignore for now */
        } else
            throw std::runtime_error("misplaced AUTO_GZIPPED packet");
#endif
        return;

    case TranslationCommand::PROBE_PATH_SUFFIXES:
        if (!response.probe_path_suffixes.IsNull() ||
            response.test_path == nullptr)
            throw std::runtime_error("misplaced PROBE_PATH_SUFFIXES packet");

        response.probe_path_suffixes = payload;
        return;

    case TranslationCommand::PROBE_SUFFIX:
        if (response.probe_path_suffixes.IsNull())
            throw std::runtime_error("misplaced PROBE_SUFFIX packet");

        if (probe_suffixes_builder.full())
            throw std::runtime_error("too many PROBE_SUFFIX packets");

        if (!CheckProbeSuffix(string_payload))
            throw std::runtime_error("malformed PROBE_SUFFIX packets");

        probe_suffixes_builder.push_back(string_payload.data);
        return;

    case TranslationCommand::AUTH_FILE:
#if TRANSLATION_ENABLE_SESSION
        if (response.HasAuth())
            throw std::runtime_error("duplicate AUTH_FILE packet");

        if (!IsValidAbsolutePath(string_payload))
            throw std::runtime_error("malformed AUTH_FILE packet");

        response.auth_file = string_payload.data;
        return;
#else
        break;
#endif

    case TranslationCommand::EXPAND_AUTH_FILE:
#if TRANSLATION_ENABLE_SESSION
        if (response.HasAuth())
            throw std::runtime_error("duplicate EXPAND_AUTH_FILE packet");

        if (!IsValidNonEmptyString(string_payload))
            throw std::runtime_error("malformed EXPAND_AUTH_FILE packet");

        if (response.regex == nullptr)
            throw std::runtime_error("misplaced EXPAND_AUTH_FILE packet");

        response.auth_file = string_payload.data;
        response.expand_auth_file = true;
        return;
#else
        break;
#endif

    case TranslationCommand::APPEND_AUTH:
#if TRANSLATION_ENABLE_SESSION
        if (!response.HasAuth() ||
            !response.append_auth.IsNull() ||
            response.expand_append_auth != nullptr)
            throw std::runtime_error("misplaced APPEND_AUTH packet");

        response.append_auth = payload;
        return;
#else
        break;
#endif

    case TranslationCommand::EXPAND_APPEND_AUTH:
#if TRANSLATION_ENABLE_SESSION
        if (response.regex == nullptr ||
            !response.HasAuth() ||
            !response.append_auth.IsNull() ||
            response.expand_append_auth != nullptr)
            throw std::runtime_error("misplaced EXPAND_APPEND_AUTH packet");

        if (!IsValidNonEmptyString(string_payload))
            throw std::runtime_error("malformed EXPAND_APPEND_AUTH packet");

        response.expand_append_auth = string_payload.data;
        return;
#else
        break;
#endif

    case TranslationCommand::EXPAND_COOKIE_HOST:
#if TRANSLATION_ENABLE_SESSION
        if (response.regex == nullptr ||
            resource_address == nullptr ||
            !resource_address->IsDefined())
            throw std::runtime_error("misplaced EXPAND_COOKIE_HOST packet");

        if (!IsValidNonEmptyString(string_payload))
            throw std::runtime_error("malformed EXPAND_COOKIE_HOST packet");

        response.cookie_host = string_payload.data;
        response.expand_cookie_host = true;
        return;
#else
        break;
#endif

    case TranslationCommand::EXPAND_BIND_MOUNT:
#if TRANSLATION_ENABLE_EXPAND
        HandleBindMount(string_payload, true, false);
        return;
#else
        break;
#endif

    case TranslationCommand::NON_BLOCKING:
#if TRANSLATION_ENABLE_RADDRESS
        if (!payload.empty())
            throw std::runtime_error("malformed NON_BLOCKING packet");

        if (lhttp_address != nullptr) {
            lhttp_address->blocking = false;
        } else
            throw std::runtime_error("misplaced NON_BLOCKING packet");

        return;
#else
        break;
#endif

    case TranslationCommand::READ_FILE:
        if (response.read_file != nullptr)
            throw std::runtime_error("duplicate READ_FILE packet");

        if (!IsValidAbsolutePath(string_payload))
            throw std::runtime_error("malformed READ_FILE packet");

        response.read_file = string_payload.data;
        return;

    case TranslationCommand::EXPAND_READ_FILE:
#if TRANSLATION_ENABLE_EXPAND
        if (response.read_file != nullptr)
            throw std::runtime_error("duplicate EXPAND_READ_FILE packet");

        if (!IsValidNonEmptyString(string_payload))
            throw std::runtime_error("malformed EXPAND_READ_FILE packet");

        response.read_file = string_payload.data;
        response.expand_read_file = true;
        return;
#else
        break;
#endif

    case TranslationCommand::EXPAND_HEADER:
#if TRANSLATION_ENABLE_HTTP && TRANSLATION_ENABLE_EXPAND
        if (response.regex == nullptr)
            throw std::runtime_error("misplaced EXPAND_HEADER packet");

        parse_header(alloc,
                     response.expand_response_headers,
                     "EXPAND_HEADER", string_payload);
        return;
#else
        break;
#endif

    case TranslationCommand::REGEX_ON_HOST_URI:
#if TRANSLATION_ENABLE_HTTP
        if (response.regex == nullptr &&
            response.inverse_regex == nullptr)
            throw std::runtime_error("REGEX_ON_HOST_URI without REGEX");

        if (response.regex_on_host_uri)
            throw std::runtime_error("duplicate REGEX_ON_HOST_URI");

        if (!payload.empty())
            throw std::runtime_error("malformed REGEX_ON_HOST_URI packet");

        response.regex_on_host_uri = true;
        return;
#else
        break;
#endif

    case TranslationCommand::SESSION_SITE:
#if TRANSLATION_ENABLE_SESSION
        response.session_site = string_payload.data;
        return;
#else
        break;
#endif

    case TranslationCommand::IPC_NAMESPACE:
        if (!payload.empty())
            throw std::runtime_error("malformed IPC_NAMESPACE packet");

        if (ns_options != nullptr) {
            ns_options->enable_ipc = true;
        } else
            throw std::runtime_error("misplaced IPC_NAMESPACE packet");

        return;

    case TranslationCommand::AUTO_DEFLATE:
        if (!payload.empty())
            throw std::runtime_error("malformed AUTO_DEFLATE packet");

        if (response.auto_deflate)
            throw std::runtime_error("misplaced AUTO_DEFLATE packet");

        response.auto_deflate = true;
        return;

    case TranslationCommand::EXPAND_HOME:
#if TRANSLATION_ENABLE_EXPAND
        translate_client_expand_home(ns_options,
#if TRANSLATION_ENABLE_JAILCGI
                                     jail,
#endif
                                     string_payload);
        return;
#else
        break;
#endif

    case TranslationCommand::EXPAND_STDERR_PATH:
#if TRANSLATION_ENABLE_EXPAND
        translate_client_expand_stderr_path(child_options,
                                            string_payload);
        return;
#else
        break;
#endif

    case TranslationCommand::REGEX_ON_USER_URI:
#if TRANSLATION_ENABLE_HTTP
        if (response.regex == nullptr &&
            response.inverse_regex == nullptr)
            throw std::runtime_error("REGEX_ON_USER_URI without REGEX");

        if (response.regex_on_user_uri)
            throw std::runtime_error("duplicate REGEX_ON_USER_URI");

        if (!payload.empty())
            throw std::runtime_error("malformed REGEX_ON_USER_URI packet");

        response.regex_on_user_uri = true;
        return;
#else
        break;
#endif

    case TranslationCommand::AUTO_GZIP:
        if (!payload.empty())
            throw std::runtime_error("malformed AUTO_GZIP packet");

        if (response.auto_gzip)
            throw std::runtime_error("misplaced AUTO_GZIP packet");

        response.auto_gzip = true;
        return;

    case TranslationCommand::INTERNAL_REDIRECT:
#if TRANSLATION_ENABLE_HTTP
        if (!response.internal_redirect.IsNull())
            throw std::runtime_error("duplicate INTERNAL_REDIRECT packet");

        response.internal_redirect = payload;
        return;
#else
        break;
#endif

    case TranslationCommand::REFENCE:
        HandleRefence(string_payload);
        return;

    case TranslationCommand::INVERSE_REGEX_UNESCAPE:
#if TRANSLATION_ENABLE_EXPAND
        if (!payload.empty())
            throw std::runtime_error("malformed INVERSE_REGEX_UNESCAPE packet");

        if (response.inverse_regex == nullptr)
            throw std::runtime_error("misplaced INVERSE_REGEX_UNESCAPE packet");

        if (response.inverse_regex_unescape)
            throw std::runtime_error("duplicate INVERSE_REGEX_UNESCAPE packet");

        response.inverse_regex_unescape = true;
        return;
#else
        break;
#endif

    case TranslationCommand::BIND_MOUNT_RW:
        HandleBindMount(string_payload, false, true);
        return;

    case TranslationCommand::EXPAND_BIND_MOUNT_RW:
#if TRANSLATION_ENABLE_EXPAND
        HandleBindMount(string_payload, true, true);
        return;
#else
        break;
#endif

    case TranslationCommand::UNTRUSTED_RAW_SITE_SUFFIX:
#if TRANSLATION_ENABLE_SESSION
        if (!IsValidNonEmptyString(string_payload) ||
            string_payload.back() == '.')
            throw std::runtime_error("malformed UNTRUSTED_RAW_SITE_SUFFIX packet");

        if (response.HasUntrusted())
            throw std::runtime_error("misplaced UNTRUSTED_RAW_SITE_SUFFIX packet");

        response.untrusted_raw_site_suffix = string_payload.data;
        return;
#else
        break;
#endif

    case TranslationCommand::MOUNT_TMPFS:
        translate_client_mount_tmpfs(ns_options, string_payload);
        return;

    case TranslationCommand::REVEAL_USER:
#if TRANSLATION_ENABLE_TRANSFORMATION
        if (!payload.empty())
            throw std::runtime_error("malformed REVEAL_USER packet");

        if (transformation == nullptr ||
            transformation->type != Transformation::Type::FILTER ||
            transformation->u.filter.reveal_user)
            throw std::runtime_error("misplaced REVEAL_USER packet");

        transformation->u.filter.reveal_user = true;
        return;
#else
        break;
#endif

    case TranslationCommand::REALM_FROM_AUTH_BASE:
#if TRANSLATION_ENABLE_SESSION
        if (!payload.empty())
            throw std::runtime_error("malformed REALM_FROM_AUTH_BASE packet");

        if (response.realm_from_auth_base)
            throw std::runtime_error("duplicate REALM_FROM_AUTH_BASE packet");

        if (response.realm != nullptr || !response.HasAuth())
            throw std::runtime_error("misplaced REALM_FROM_AUTH_BASE packet");

        response.realm_from_auth_base = true;
        return;
#else
        break;
#endif

    case TranslationCommand::FORBID_USER_NS:
        if (child_options == nullptr || child_options->forbid_user_ns)
            throw std::runtime_error("misplaced FORBID_USER_NS packet");

        if (!payload.empty())
            throw std::runtime_error("malformed FORBID_USER_NS packet");

        child_options->forbid_user_ns = true;
        return;

    case TranslationCommand::NO_NEW_PRIVS:
        if (child_options == nullptr || child_options->no_new_privs)
            throw std::runtime_error("misplaced NO_NEW_PRIVS packet");

        if (!payload.empty())
            throw std::runtime_error("malformed NO_NEW_PRIVS packet");

        child_options->no_new_privs = true;
        return;

    case TranslationCommand::CGROUP:
        if (child_options == nullptr ||
            child_options->cgroup.name != nullptr)
            throw std::runtime_error("misplaced CGROUP packet");

        if (!valid_view_name(string_payload.data))
            throw std::runtime_error("malformed CGROUP packet");

        child_options->cgroup.name = string_payload.data;
        return;

    case TranslationCommand::CGROUP_SET:
        HandleCgroupSet(string_payload);
        return;

    case TranslationCommand::EXTERNAL_SESSION_MANAGER:
#if TRANSLATION_ENABLE_SESSION
        if (!IsValidNonEmptyString(string_payload))
            throw std::runtime_error("malformed EXTERNAL_SESSION_MANAGER packet");

        if (response.external_session_manager != nullptr)
            throw std::runtime_error("duplicate EXTERNAL_SESSION_MANAGER packet");

        response.external_session_manager = http_address =
            http_address_parse(alloc, string_payload.data);

        address_list = &http_address->addresses;
        default_port = http_address->GetDefaultPort();
        return;
#else
        break;
#endif

    case TranslationCommand::EXTERNAL_SESSION_KEEPALIVE: {
#if TRANSLATION_ENABLE_SESSION
        const uint16_t *value = (const uint16_t *)payload.data;
        if (payload.size != sizeof(*value) || *value == 0)
            throw std::runtime_error("malformed EXTERNAL_SESSION_KEEPALIVE packet");

        if (response.external_session_manager == nullptr)
            throw std::runtime_error("misplaced EXTERNAL_SESSION_KEEPALIVE packet");

        if (response.external_session_keepalive != std::chrono::seconds::zero())
            throw std::runtime_error("duplicate EXTERNAL_SESSION_KEEPALIVE packet");

        response.external_session_keepalive = std::chrono::seconds(*value);
        return;
#else
        break;
#endif
    }

    case TranslationCommand::BIND_MOUNT_EXEC:
        HandleBindMount(string_payload, false, false, true);
        return;

    case TranslationCommand::EXPAND_BIND_MOUNT_EXEC:
#if TRANSLATION_ENABLE_EXPAND
        HandleBindMount(string_payload, true, false, true);
        return;
#else
        break;
#endif

    case TranslationCommand::STDERR_NULL:
        if (!payload.empty())
            throw std::runtime_error("malformed STDERR_NULL packet");

        if (child_options == nullptr || child_options->stderr_path != nullptr)
            throw std::runtime_error("misplaced STDERR_NULL packet");

        if (child_options->stderr_null)
            throw std::runtime_error("duplicate STDERR_NULL packet");

        child_options->stderr_null = true;
        return;

    case TranslationCommand::EXECUTE:
#if TRANSLATION_ENABLE_EXECUTE
        if (!IsValidAbsolutePath(string_payload))
            throw std::runtime_error("malformed EXECUTE packet");

        if (response.execute != nullptr)
            throw std::runtime_error("duplicate EXECUTE packet");

        response.execute = string_payload.data;
        args_builder = response.args;
        return;
#else
        break;
#endif

    case TranslationCommand::POOL:
        if (!IsValidNonEmptyString(string_payload))
            throw std::runtime_error("malformed POOL packet");

        response.pool = string_payload.data;
        return;

    case TranslationCommand::MESSAGE:
#if TRANSLATION_ENABLE_HTTP
        if (string_payload.size > 1024 ||
            !IsValidNonEmptyString(string_payload))
            throw std::runtime_error("malformed MESSAGE packet");

        response.message = string_payload.data;
        return;
#else
        break;
#endif

    case TranslationCommand::CANONICAL_HOST:
        if (!IsValidNonEmptyString(string_payload))
            throw std::runtime_error("malformed CANONICAL_HOST packet");

        response.canonical_host = string_payload.data;
        return;

    case TranslationCommand::SHELL:
#if TRANSLATION_ENABLE_EXECUTE
        if (!IsValidAbsolutePath(string_payload))
            throw std::runtime_error("malformed SHELL packet");

        if (response.shell != nullptr)
            throw std::runtime_error("duplicate SHELL packet");

        response.shell = string_payload.data;
        return;
#else
        break;
#endif

    case TranslationCommand::TOKEN:
        if (HasNullByte(payload))
            throw std::runtime_error("malformed TOKEN packet");
        response.token = string_payload.data;
        return;

    case TranslationCommand::STDERR_PATH_JAILED:
        translate_client_stderr_path(child_options, payload, true);
        return;

    case TranslationCommand::UMASK:
        HandleUmask(payload);
        return;

    case TranslationCommand::CGROUP_NAMESPACE:
        if (!payload.empty())
            throw std::runtime_error("malformed CGROUP_NAMESPACE packet");

        if (ns_options != nullptr) {
            if (ns_options->enable_cgroup)
                throw std::runtime_error("duplicate CGROUP_NAMESPACE packet");

            ns_options->enable_cgroup = true;
        } else
            throw std::runtime_error("misplaced CGROUP_NAMESPACE packet");

        return;

    case TranslationCommand::REDIRECT_FULL_URI:
#if TRANSLATION_ENABLE_HTTP
        if (!payload.empty())
            throw std::runtime_error("malformed REDIRECT_FULL_URI packet");

        if (response.base == nullptr)
            throw std::runtime_error("REDIRECT_FULL_URI without BASE");

        if (!response.easy_base)
            throw std::runtime_error("REDIRECT_FULL_URI without EASY_BASE");

        if (response.redirect_full_uri)
            throw std::runtime_error("duplicate REDIRECT_FULL_URI packet");

        response.redirect_full_uri = true;
        return;
#else
        break;
#endif

    case TranslationCommand::HTTPS_ONLY:
#if TRANSLATION_ENABLE_HTTP
        if (response.https_only != 0)
            throw std::runtime_error("duplicate HTTPS_ONLY packet");

        if (payload.size == sizeof(response.https_only)) {
            response.https_only = *(const uint16_t *)payload.data;
            if (response.https_only == 0)
                /* zero in the packet means "default port", but we
                   change it here to 443 because in the variable, zero
                   means "not set" */
                response.https_only = 443;
        } else if (payload.empty())
            response.https_only = 443;
        else
            throw std::runtime_error("malformed HTTPS_ONLY packet");

        return;
#else
        break;
#endif

    case TranslationCommand::FORBID_MULTICAST:
        if (child_options == nullptr || child_options->forbid_multicast)
            throw std::runtime_error("misplaced FORBID_MULTICAST packet");

        if (!payload.empty())
            throw std::runtime_error("malformed FORBID_MULTICAST packet");

        child_options->forbid_multicast = true;
        return;

    case TranslationCommand::FORBID_BIND:
        if (child_options == nullptr || child_options->forbid_bind)
            throw std::runtime_error("misplaced FORBID_BIND packet");

        if (!payload.empty())
            throw std::runtime_error("malformed FORBID_BIND packet");

        child_options->forbid_bind = true;
        return;

    case TranslationCommand::NETWORK_NAMESPACE_NAME:
        if (!IsValidName(string_payload))
            throw std::runtime_error("malformed NETWORK_NAMESPACE_NAME packet");

        if (ns_options == nullptr)
            throw std::runtime_error("misplaced NETWORK_NAMESPACE_NAME packet");

        if (ns_options->network_namespace != nullptr)
            throw std::runtime_error("duplicate NETWORK_NAMESPACE_NAME packet");

        if (ns_options->enable_network)
            throw std::runtime_error("Can't combine NETWORK_NAMESPACE_NAME with NETWORK_NAMESPACE");

        ns_options->network_namespace = string_payload.data;
        return;

    case TranslationCommand::MOUNT_ROOT_TMPFS:
        translate_client_mount_root_tmpfs(ns_options, payload.size);
        return;

    case TranslationCommand::CHILD_TAG:
        if (HasNullByte(payload))
            throw std::runtime_error("malformed CHILD_TAG packet");

        if (child_options == nullptr)
            throw std::runtime_error("misplaced CHILD_TAG packet");

        if (child_options->tag != nullptr)
            throw std::runtime_error("duplicate CHILD_TAG packet");

        child_options->tag = string_payload.data;
        return;

    case TranslationCommand::CERTIFICATE:
#if TRANSLATION_ENABLE_RADDRESS
        if (http_address == nullptr || !http_address->ssl)
            throw std::runtime_error("misplaced CERTIFICATE packet");

        if (http_address->certificate != nullptr)
            throw std::runtime_error("duplicate CERTIFICATE packet");

        if (!IsValidName(string_payload))
            throw std::runtime_error("malformed CERTIFICATE packet");

        http_address->certificate = string_payload.data;
        return;
#else
        break;
#endif

    case TranslationCommand::UNCACHED:
#if TRANSLATION_ENABLE_RADDRESS
        if (resource_address == nullptr)
            throw std::runtime_error("misplaced UNCACHED packet");
#endif

        if (response.uncached)
            throw std::runtime_error("duplicate UNCACHED packet");

        response.uncached = true;
        return;

    case TranslationCommand::PID_NAMESPACE_NAME:
        if (!IsValidName(string_payload))
            throw std::runtime_error("malformed PID_NAMESPACE_NAME packet");

        if (ns_options == nullptr)
            throw std::runtime_error("misplaced PID_NAMESPACE_NAME packet");

        if (ns_options->pid_namespace != nullptr)
            throw std::runtime_error("duplicate PID_NAMESPACE_NAME packet");

        if (ns_options->enable_pid)
            throw std::runtime_error("Can't combine PID_NAMESPACE_NAME with PID_NAMESPACE");

        ns_options->pid_namespace = string_payload.data;
        return;

    case TranslationCommand::SUBST_YAML_FILE:
#if TRANSLATION_ENABLE_TRANSFORMATION
        HandleSubstYamlFile(string_payload);
        return;
#else
        break;
#endif

    case TranslationCommand::SUBST_ALT_SYNTAX:
#if TRANSLATION_ENABLE_TRANSFORMATION
        if (!payload.empty())
            throw std::runtime_error("malformed SUBST_ALT_SYNTAX packet");

        if (response.subst_alt_syntax)
            throw std::runtime_error("duplicate SUBST_ALT_SYNTAX packet");

        response.subst_alt_syntax = true;
        return;
#else
        break;
#endif
    }

    throw FormatRuntimeError("unknown translation packet: %u", command);
}

inline TranslateParser::Result
TranslateParser::HandlePacket(TranslationCommand command,
                              ConstBuffer<void> payload)
{
    assert(payload != nullptr);

    if (command == TranslationCommand::BEGIN) {
        if (begun)
            throw std::runtime_error("double BEGIN from translation server");
    } else {
        if (!begun)
            throw std::runtime_error("no BEGIN from translation server");
    }

    switch (command) {
    case TranslationCommand::END:
        FinishTranslateResponse(alloc, response,
                                {probe_suffixes_builder.raw(), probe_suffixes_builder.size()});

#if TRANSLATION_ENABLE_WIDGET
        FinishView();
#endif
        return Result::DONE;

    case TranslationCommand::BEGIN:
        begun = true;
        response.Clear();
        previous_command = command;
#if TRANSLATION_ENABLE_RADDRESS
        resource_address = &response.address;
#endif
        probe_suffixes_builder.clear();
#if TRANSLATION_ENABLE_JAILCGI
        jail = nullptr;
#endif
#if TRANSLATION_ENABLE_EXECUTE
        SetChildOptions(response.child_options);
#else
        child_options = nullptr;
        ns_options = nullptr;
        mount_list = nullptr;
#endif
#if TRANSLATION_ENABLE_RADDRESS
        file_address = nullptr;
        http_address = nullptr;
        cgi_address = nullptr;
        nfs_address = nullptr;
        lhttp_address = nullptr;
        address_list = nullptr;
#endif

#if TRANSLATION_ENABLE_WIDGET
        response.views = alloc.New<WidgetView>(nullptr);
        view = nullptr;
        widget_view_tail = &response.views->next;
#endif

#if TRANSLATION_ENABLE_TRANSFORMATION
        transformation = nullptr;
        transformation_tail = &response.views->transformation;
#endif

        if (payload.size >= sizeof(uint8_t))
            response.protocol_version = *(const uint8_t *)payload.data;

        return Result::MORE;

    default:
        HandleRegularPacket(command, payload);
        return Result::MORE;
    }
}

TranslateParser::Result
TranslateParser::Process()
{
    if (!reader.IsComplete())
        /* need more data */
        return Result::MORE;

    return HandlePacket(reader.GetCommand(), reader.GetPayload());
}
