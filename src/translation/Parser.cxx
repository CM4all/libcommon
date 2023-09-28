// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Parser.hxx"
#include "Response.hxx"
#if TRANSLATION_ENABLE_TRANSFORMATION
#include "translation/Transformation.hxx"
#include "bp/XmlProcessor.hxx"
#include "bp/CssProcessor.hxx"
#endif
#if TRANSLATION_ENABLE_WIDGET
#include "widget/Class.hxx"
#include "widget/View.hxx"
#endif
#if TRANSLATION_ENABLE_RADDRESS
#include "translation/Layout.hxx"
#include "file/Address.hxx"
#include "delegate/Address.hxx"
#include "http/local/Address.hxx"
#include "http/Address.hxx"
#include "cgi/Address.hxx"
#include "nfs/Address.hxx"
#include "uri/Base.hxx"
#endif
#include "spawn/ChildOptions.hxx"
#include "spawn/Mount.hxx"
#include "spawn/ResourceLimits.hxx"
#if TRANSLATION_ENABLE_HTTP
#include "net/AllocatedSocketAddress.hxx"
#include "net/AddressInfo.hxx"
#include "net/Parser.hxx"
#include "http/Status.hxx"
#endif
#include "lib/fmt/RuntimeError.hxx"
#include "util/CharUtil.hxx"
#include "util/Compiler.h"
#include "util/SpanCast.hxx"
#include "util/StringCompare.hxx"
#include "util/StringSplit.hxx"
#include "util/StringVerify.hxx"
#include "util/StringListVerify.hxx"

#if TRANSLATION_ENABLE_HTTP
#include "http/HeaderName.hxx"
#endif

#include <algorithm>

#include <assert.h>
#include <string.h>

using std::string_view_literals::operator""sv;

inline bool
TranslateParser::HasArgs() const noexcept
{
#if TRANSLATION_ENABLE_RADDRESS
	if (cgi_address != nullptr || lhttp_address != nullptr)
		return true;
#endif

#if TRANSLATION_ENABLE_EXECUTE
	if (response.execute != nullptr)
		return true;
#endif

	return false;
}

void
TranslateParser::SetChildOptions(ChildOptions &_child_options)
{
	child_options = &_child_options;
	ns_options = &child_options->ns;
	mount_list = ns_options->mount.mounts.before_begin();
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

void
TranslateParser::FinishAddressList() noexcept
{
	if (address_list == nullptr)
		return;

	if (!address_list_builder.empty()) {
		*address_list = address_list_builder.Finish(alloc);
		address_list_builder.clear();
	}


	address_list = nullptr;
}

#endif

/*
 * receive response
 *
 */

[[gnu::pure]]
static bool
HasNullByte(std::span<const std::byte> p) noexcept
{
	return std::find(p.begin(), p.end(), std::byte{0}) != p.end();
}

[[gnu::pure]]
static bool
IsValidString(std::string_view s) noexcept
{
	return !HasNullByte(AsBytes(s));
}

[[gnu::pure]]
static bool
IsValidNonEmptyString(std::string_view s) noexcept
{
	return !s.empty() && IsValidString(s);
}

static constexpr bool
IsValidNameChar(char ch)
{
	return IsAlphaNumericASCII(ch) || ch == '-' || ch == '_';
}

static constexpr bool
IsValidName(std::string_view s) noexcept
{
	return CheckCharsNonEmpty(s, IsValidNameChar);
}

[[gnu::pure]]
static bool
IsValidAbsolutePath(std::string_view p) noexcept
{
	return IsValidNonEmptyString(p) && p.front() == '/';
}

#if TRANSLATION_ENABLE_HTTP

[[gnu::pure]]
static bool
IsValidAbsoluteUriPath(std::string_view p) noexcept
{
	return IsValidAbsolutePath(p);
}

#endif

#if TRANSLATION_ENABLE_SESSION

/**
 * Is this a valid cookie value character according to RFC 6265 4.1.1?
 */
static constexpr bool
IsValidCookieValueChar(char ch) noexcept
{
	return IsASCII(ch) && !IsWhitespaceFast(ch) && ch != 0x7f &&
		ch != '"' && ch != ',' && ch != ';' && ch != '\\';
}

static constexpr bool
IsValidCookieValue(std::string_view s) noexcept
{
	return CheckChars(s, IsValidCookieValueChar);
}

static constexpr bool
IsValidLowerHeaderNameChar(char ch) noexcept
{
	return IsLowerAlphaASCII(ch) || IsDigitASCII(ch) || ch == '-';
}

static constexpr bool
IsValidLowerHeaderName(std::string_view s) noexcept
{
	return CheckCharsNonEmpty(s, IsValidLowerHeaderNameChar);
}

#endif

#if TRANSLATION_ENABLE_TRANSFORMATION

template<typename... Args>
Transformation *
TranslateParser::AddTransformation(Args&&... args) noexcept
{
	auto t = alloc.New<Transformation>(std::forward<Args>(args)...);

	filter = nullptr;
	transformation = t;

	transformation_tail = IntrusiveForwardList<Transformation>::insert_after(transformation_tail, *t);

	return t;
}

ResourceAddress *
TranslateParser::AddFilter()
{
	auto *t = AddTransformation(FilterTransformation{});
	filter = &t->u.filter;
	return &t->u.filter.address;
}

void
TranslateParser::AddSubstYamlFile(const char *prefix,
				  const char *file_path,
				  const char *map_path) noexcept
{
	AddTransformation(SubstTransformation{prefix, file_path, map_path});
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
	assert(!response.views.empty());

	FinishAddressList();

	WidgetView *v = view;
	if (view == nullptr) {
		v = &response.views.front();

		const ResourceAddress *address = &response.address;
		if (address->IsDefined() && !v->address.IsDefined()) {
			/* no address yet: copy address from response */
			v->address.CopyFrom(alloc, *address);
			v->filter_4xx = response.filter_4xx;
		}

		v->request_header_forward = response.request_header_forward;
		v->response_header_forward = response.response_header_forward;
	} else {
		if (!v->address.IsDefined() && v != &response.views.front())
			/* no address yet: inherits settings from the default view */
			v->InheritFrom(alloc, response.views.front());
	}

	v->address.Check();
}

inline void
TranslateParser::AddView(const char *name)
{
	FinishView();

	assert(address_list == nullptr);

	auto new_view = alloc.New<WidgetView>(name);
	new_view->request_header_forward = response.request_header_forward;
	new_view->response_header_forward = response.response_header_forward;

	view = new_view;
	widget_view_tail = response.views.insert_after(widget_view_tail, *new_view);
	resource_address = &new_view->address;
	child_options = nullptr;
	ns_options = nullptr;
	file_address = nullptr;
	http_address = nullptr;
	cgi_address = nullptr;
	nfs_address = nullptr;
	lhttp_address = nullptr;
	transformation_tail = new_view->transformations.before_begin();
	transformation = nullptr;
	filter = nullptr;
}

#endif

#if TRANSLATION_ENABLE_HTTP

static void
parse_header_forward(HeaderForwardSettings *settings,
		     std::span<const std::byte> _payload)
{
	using namespace BengProxy;

	const auto payload =
		FromBytesFloor<const HeaderForwardPacket>(_payload);

	if (_payload.size() % sizeof(payload.front()) != 0)
		throw std::runtime_error("malformed header forward packet");

	for (const auto &packet : payload) {
		if (packet.group < int(HeaderGroup::ALL) ||
		    packet.group >= int(HeaderGroup::MAX) ||
		    (packet.mode != unsigned(HeaderForwardMode::NO) &&
		     packet.mode != unsigned(HeaderForwardMode::YES) &&
		     packet.mode != unsigned(HeaderForwardMode::BOTH) &&
		     packet.mode != unsigned(HeaderForwardMode::MANGLE)) ||
		    packet.reserved != 0)
			throw std::runtime_error("malformed header forward packet");

		if (HeaderGroup(packet.group) == HeaderGroup::ALL) {
			for (unsigned i = 0; i < unsigned(HeaderGroup::MAX); ++i)
				if (HeaderGroup(i) != HeaderGroup::SECURE &&
				    HeaderGroup(i) != HeaderGroup::AUTH &&
				    HeaderGroup(i) != HeaderGroup::SSL)
					settings->modes[i] = HeaderForwardMode(packet.mode);
		} else
			settings->modes[packet.group] = HeaderForwardMode(packet.mode);
	}
}

static void
parse_header(AllocatorPtr alloc,
	     KeyValueList &headers, const char *packet_name,
	     std::string_view payload)
{
	const auto [_name, value] = Split(payload, ':');
	if (_name.empty() || value.data() == nullptr ||
	    !IsValidString(payload))
		throw FmtRuntimeError("malformed {} packet", packet_name);

	const char *name = alloc.DupToLower(_name);

	if (!http_header_name_valid(name))
		throw FmtRuntimeError("malformed name in {} packet", packet_name);
	else if (http_header_is_hop_by_hop(name))
		throw FmtRuntimeError("hop-by-hop {} packet", packet_name);

	headers.Add(alloc, name, value.data());
}

#endif

/**
 * Final fixups for the response before it is passed to the handler.
 *
 * Throws std::runtime_error on error.
 */
static void
FinishTranslateResponse(AllocatorPtr alloc,
#if TRANSLATION_ENABLE_RADDRESS
			const char *base_suffix,
			std::shared_ptr<std::vector<TranslationLayoutItem>> &&layout_items,
#endif
			TranslateResponse &response,
			std::span<const char *const> probe_suffixes)
{
#if TRANSLATION_ENABLE_RADDRESS
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
	} else if (response.address.type == ResourceAddress::Type::LOCAL) {
		auto &file = response.address.GetFile();

		if (file.delegate != nullptr) {
		} else if (response.base != nullptr) {
			if (response.easy_base) {
				if (file.expand_path)
					throw std::runtime_error("Cannot use EASY_BASE with EXPAND_PATH");

				if (!is_base(file.path))
					throw std::runtime_error("PATH is not a valid base for EASY_BASE");

				file.base = file.path;
				file.path = ".";
			} else {
				assert(base_suffix != nullptr);

				if (!file.SplitBase(alloc, base_suffix))
					throw std::runtime_error("Base mismatch");
			}
		}
	}

	response.address.Check();

	if (response.layout.data() != nullptr) {
		assert(layout_items);

		if (layout_items->empty())
			throw std::runtime_error("empty LAYOUT");

		response.layout_items = std::move(layout_items);
	}

	/* the ResourceAddress::IsValidBase() check works only after
	   the transformations above, in particular the
	   FileAddress::SplitBase() call */
	if (response.easy_base && !response.address.IsValidBase())
		/* EASY_BASE was enabled, but the resource address does not
		   end with a slash, thus LoadBase() cannot work */
		throw std::runtime_error("Invalid base address");
#endif

#if TRANSLATION_ENABLE_HTTP
	/* these lists are in reverse order because new items were added
	   to the front; reverse them now */
	response.request_headers.Reverse();
	response.response_headers.Reverse();
#endif

	if (probe_suffixes.data() != nullptr)
		response.probe_suffixes = alloc.Dup(probe_suffixes);

	if (response.probe_path_suffixes.data() != nullptr &&
	    response.probe_suffixes.empty())
		throw std::runtime_error("PROBE_PATH_SUFFIX without PROBE_SUFFIX");

#if TRANSLATION_ENABLE_HTTP
	if (response.internal_redirect.data() != nullptr &&
	    response.uri == nullptr)
		throw std::runtime_error("INTERNAL_REDIRECT without URI");

	if (response.internal_redirect.data() != nullptr &&
	    response.want_full_uri.data() != nullptr)
		throw std::runtime_error("INTERNAL_REDIRECT conflicts with WANT_FULL_URI");
#endif
}

[[gnu::pure]]
static bool
translate_client_check_pair(std::string_view payload) noexcept
{
	return !payload.empty() && payload.front() != '=' &&
		IsValidString(payload) &&
		payload.substr(1).find('=') != payload.npos;
}

static void
translate_client_check_pair(const char *name, std::string_view payload)
{
	if (!translate_client_check_pair(payload))
		throw FmtRuntimeError("malformed {} packet", name);
}

static void
translate_client_pair(AllocatorPtr alloc,
		      ExpandableStringList::Builder &builder,
		      const char *name, std::string_view payload)
{
	translate_client_check_pair(name, payload);

	builder.Add(alloc, payload.data(), false);
}

#if TRANSLATION_ENABLE_EXPAND

static void
translate_client_expand_pair(ExpandableStringList::Builder &builder,
			     const char *name,
			     std::string_view payload)
{
	if (!builder.CanSetExpand())
		throw FmtRuntimeError("misplaced {} packet", name);

	translate_client_check_pair(name, payload);

	builder.SetExpand(payload.data());
}

#endif

static void
translate_client_pivot_root(NamespaceOptions *ns, std::string_view payload)
{
	if (!IsValidAbsolutePath(payload))
		throw std::runtime_error("malformed PIVOT_ROOT packet");

	if (ns == nullptr || ns->mount.pivot_root != nullptr ||
	    ns->mount.mount_root_tmpfs)
		throw std::runtime_error("misplaced PIVOT_ROOT packet");

	ns->mount.pivot_root = payload.data();
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

	ns->mount.mount_root_tmpfs = true;
}

static void
translate_client_home(NamespaceOptions *ns,
		      std::string_view payload)
{
	if (!IsValidAbsolutePath(payload))
		throw std::runtime_error("malformed HOME packet");

	bool ok = false;

	if (ns != nullptr && ns->mount.home == nullptr) {
		ns->mount.home = payload.data();
		ok = true;
	}

	if (!ok)
		throw std::runtime_error("misplaced HOME packet");
}

#if TRANSLATION_ENABLE_EXPAND

static void
translate_client_expand_home(NamespaceOptions *ns,
			     std::string_view payload)
{
	if (!IsValidAbsolutePath(payload))
		throw std::runtime_error("malformed EXPAND_HOME packet");

	if (ns == nullptr)
		throw std::runtime_error{"misplaced EXPAND_HOME packet"};

	if (ns->mount.expand_home)
		throw std::runtime_error{"duplicate EXPAND_HOME packet"};

	ns->mount.expand_home = true;
	ns->mount.home = payload.data();
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

	ns->mount.mount_proc = true;
}

static void
translate_client_mount_tmp_tmpfs(NamespaceOptions *ns,
				 std::string_view payload)
{
	if (!IsValidString(payload))
		throw std::runtime_error("malformed MOUNT_TMP_TMPFS packet");

	if (ns == nullptr || ns->mount.mount_tmp_tmpfs != nullptr)
		throw std::runtime_error("misplaced MOUNT_TMP_TMPFS packet");

	ns->mount.mount_tmp_tmpfs = payload.data() != nullptr
		? payload.data()
		: "";
}

inline void
TranslateParser::HandleMountHome(std::string_view payload)
{
	if (!IsValidAbsolutePath(payload))
		throw std::runtime_error("malformed MOUNT_HOME packet");

	if (ns_options == nullptr || ns_options->mount.home == nullptr)
		throw std::runtime_error("misplaced MOUNT_HOME packet");

	if (ns_options->mount.HasMountHome())
		throw std::runtime_error{"duplicate MOUNT_HOME packet"};

	auto *m = alloc.New<Mount>(/* skip the slash to make it relative */
		ns_options->mount.home + 1,
		payload.data(),
		true, true);

#if TRANSLATION_ENABLE_EXPAND
	m->expand_source = ns_options->mount.expand_home;
#endif

	mount_list = ns_options->mount.mounts.insert_after(mount_list, *m);

	assert(ns_options->mount.HasMountHome());
}

inline void
TranslateParser::HandleMountTmpfs(std::string_view payload, bool writable)
{
	if (!IsValidAbsolutePath(payload) ||
	    /* not allowed for /tmp, use MOUNT_TMP_TMPFS
	       instead! */
	    payload == "/tmp"sv)
		throw std::runtime_error("malformed MOUNT_TMPFS packet");

	if (ns_options == nullptr)
		throw std::runtime_error("misplaced MOUNT_TMPFS packet");

	auto *m = alloc.New<Mount>(Mount::Tmpfs{}, payload.data(), writable);
	mount_list = ns_options->mount.mounts.insert_after(mount_list, *m);
}

inline void
TranslateParser::HandleMountNamedTmpfs(std::string_view payload)
{
	const auto [source, target] = Split(payload, '\0');
	if (!IsValidName(source) ||
	    !IsValidAbsolutePath(target))
		throw std::runtime_error("malformed MOUNT_NAMED_TMPFS packet");

	if (ns_options == nullptr)
		throw std::runtime_error("misplaced MOUNT_NAMED_TMPFS packet");

	auto *m = alloc.New<Mount>(Mount::NamedTmpfs{},
				   source.data(),
				   target.data(),
				   true);

	mount_list = ns_options->mount.mounts.insert_after(mount_list, *m);
}

inline void
TranslateParser::HandleBindMount(std::string_view payload,
				 bool expand, bool writable, bool exec,
				 bool file)
{
	const auto [source, target] = Split(payload, '\0');
	if (!IsValidAbsolutePath(source) ||
	    !IsValidAbsolutePath(target))
		throw std::runtime_error("malformed BIND_MOUNT packet");

	if (ns_options == nullptr)
		throw std::runtime_error("misplaced BIND_MOUNT packet");

	auto *m = alloc.New<Mount>(/* skip the slash to make it relative */
		source.data() + 1,
		target.data(),
		writable, exec);
#if TRANSLATION_ENABLE_EXPAND
	m->expand_source = expand;
#else
	(void)expand;
#endif

	if (file)
		m->type = Mount::Type::BIND_FILE;

	mount_list = ns_options->mount.mounts.insert_after(mount_list, *m);
}

inline void
TranslateParser::HandleWriteFile(std::string_view payload)
{
	if (ns_options == nullptr)
		throw std::runtime_error("misplaced WRITE_FILE packet");

	const auto [path, contents] = Split(payload, '\0');
	if (!IsValidAbsolutePath(path) || !IsValidString(contents))
		throw std::runtime_error("malformed WRITE_FILE packet");

	auto *m = alloc.New<Mount>(Mount::WriteFile{},
				   path.data(),
				   contents.data());

	mount_list = ns_options->mount.mounts.insert_after(mount_list, *m);
}

static void
translate_client_uts_namespace(NamespaceOptions *ns,
			       std::string_view payload)
{
	if (!IsValidNonEmptyString(payload))
		throw std::runtime_error("malformed MOUNT_UTS_NAMESPACE packet");

	if (ns == nullptr || ns->hostname != nullptr)
		throw std::runtime_error("misplaced MOUNT_UTS_NAMESPACE packet");

	ns->hostname = payload.data();
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
				std::span<const std::byte> payload)
{
	if (response.file_not_found.data() != nullptr)
		throw std::runtime_error("duplicate FILE_NOT_FOUND packet");

	if (response.test_path == nullptr) {
		switch (response.address.type) {
		case ResourceAddress::Type::NONE:
			throw std::runtime_error("FILE_NOT_FOUND without resource address");

		case ResourceAddress::Type::HTTP:
		case ResourceAddress::Type::PIPE:
			throw std::runtime_error("FILE_NOT_FOUND not compatible with resource address");

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
TranslateParser::HandleContentTypeLookup(std::span<const std::byte> payload)
{
	const char *content_type;
	std::span<const std::byte> *content_type_lookup;

	if (file_address != nullptr) {
		content_type = file_address->content_type;
		content_type_lookup = &file_address->content_type_lookup;
	} else if (nfs_address != nullptr) {
		content_type = nfs_address->content_type;
		content_type_lookup = &nfs_address->content_type_lookup;
	} else
		throw std::runtime_error("misplaced CONTENT_TYPE_LOOKUP");

	if (content_type_lookup->data() != nullptr)
		throw std::runtime_error("duplicate CONTENT_TYPE_LOOKUP");

	if (content_type != nullptr)
		throw std::runtime_error("CONTENT_TYPE/CONTENT_TYPE_LOOKUP conflict");

	*content_type_lookup = payload;
}

static void
translate_client_enotdir(TranslateResponse &response,
			 std::span<const std::byte> payload)
{
	if (response.enotdir.data() != nullptr)
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
				 std::span<const std::byte> payload)
{
	if (response.directory_index.data() != nullptr)
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
				  std::span<const std::byte> payload)
{
	if (response.expires_relative > std::chrono::seconds::zero())
		throw std::runtime_error("duplicate EXPIRES_RELATIVE");

	if (payload.size() != sizeof(uint32_t))
		throw std::runtime_error("malformed EXPIRES_RELATIVE");

	response.expires_relative = std::chrono::seconds(*(const uint32_t *)(const void *)payload.data());
}

static void
translate_client_expires_relative_with_query(TranslateResponse &response,
					     std::span<const std::byte> payload)
{
	if (response.expires_relative_with_query > std::chrono::seconds::zero())
		throw std::runtime_error("duplicate EXPIRES_RELATIVE_WITH_QUERY");

	if (payload.size() != sizeof(uint32_t))
		throw std::runtime_error("malformed EXPIRES_RELATIVE_WITH_QUERY");

	response.expires_relative_with_query = std::chrono::seconds(*(const uint32_t *)(const void *)payload.data());
}

static void
translate_client_stderr_path(ChildOptions *child_options,
			     const std::string_view path,
			     bool jailed)
{
	if (!IsValidAbsolutePath(path))
		throw std::runtime_error("malformed STDERR_PATH packet");

	if (child_options == nullptr || child_options->stderr_null)
		throw std::runtime_error("misplaced STDERR_PATH packet");

	if (child_options->stderr_path != nullptr)
		throw std::runtime_error("duplicate STDERR_PATH packet");

	child_options->stderr_path = path.data();
	child_options->stderr_jailed = jailed;
}

#if TRANSLATION_ENABLE_EXPAND

static void
translate_client_expand_stderr_path(ChildOptions *child_options,
				    std::string_view payload)
{
	if (!IsValidNonEmptyString(payload))
		throw std::runtime_error("malformed EXPAND_STDERR_PATH packet");

	if (child_options == nullptr)
		throw std::runtime_error("misplaced EXPAND_STDERR_PATH packet");

	if (child_options->expand_stderr_path != nullptr)
		throw std::runtime_error("duplicate EXPAND_STDERR_PATH packet");

	child_options->expand_stderr_path = payload.data();
}

#endif

inline void
TranslateParser::HandleUidGid(std::span<const std::byte> _payload)
{
	if (child_options == nullptr || !child_options->uid_gid.IsEmpty())
		throw std::runtime_error("misplaced UID_GID packet");

	UidGid &uid_gid = child_options->uid_gid;

	constexpr size_t min_size = sizeof(int) * 2;
	const size_t max_size = min_size + sizeof(int) * uid_gid.groups.max_size();

	if (_payload.size() < min_size || _payload.size() > max_size ||
	    _payload.size() % sizeof(int) != 0)
		throw std::runtime_error("malformed UID_GID packet");

	const auto payload = FromBytesFloor<const int>(_payload);
	uid_gid.uid = payload[0];
	uid_gid.gid = payload[1];

	size_t n_groups = payload.size() - 2;
	std::copy(std::next(payload.begin(), 2), payload.end(),
		  uid_gid.groups.begin());
	if (n_groups < uid_gid.groups.max_size())
		uid_gid.groups[n_groups] = 0;
}

inline void
TranslateParser::HandleUmask(std::span<const std::byte> payload)
{
	typedef uint16_t value_type;

	if (child_options == nullptr)
		throw std::runtime_error("misplaced UMASK packet");

	if (child_options->umask >= 0)
		throw std::runtime_error("duplicate UMASK packet");

	if (payload.size() != sizeof(value_type))
		throw std::runtime_error("malformed UMASK packet");

	auto umask = *(const uint16_t *)(const void *)payload.data();
	if (umask & ~0777)
		throw std::runtime_error("malformed UMASK packet");

	child_options->umask = umask;
}

static constexpr bool
IsValidCgroupNameChar(char ch) noexcept
{
	return IsLowerAlphaASCII(ch) || ch == '_';
}

static constexpr bool
IsValidCgroupName(std::string_view s) noexcept
{
	return CheckCharsNonEmpty(s, IsValidCgroupNameChar);
}

static constexpr bool
IsValidCgroupAttributeNameChar(char ch) noexcept
{
	return IsLowerAlphaASCII(ch) || ch == '_';
}

static constexpr bool
IsValidCgroupAttributeNameSegment(std::string_view s) noexcept
{
	return CheckCharsNonEmpty(s, IsValidCgroupAttributeNameChar);
}

static constexpr bool
IsValidCgroupAttributeName(std::string_view s) noexcept
{
	return IsNonEmptyListOf(s, '.', IsValidCgroupAttributeNameSegment);
}

[[gnu::pure]]
static bool
IsValidCgroupSetName(std::string_view name) noexcept
{
	const auto [controller, attribute] = Split(name, '.');
	if (!IsValidCgroupName(controller) ||
	    !IsValidCgroupAttributeName(attribute))
		return false;

	if (controller == "cgroup"sv)
		/* this is not a controller, this is a core cgroup
		   attribute */
		return false;

	return true;
}

[[gnu::pure]]
static bool
IsValidCgroupSetValue(std::string_view value) noexcept
{
	return !value.empty() && value.find('/') == value.npos;
}

[[gnu::pure]]
static std::pair<std::string_view, std::string_view>
ParseCgroupSet(std::string_view payload)
{
	if (!IsValidString(payload))
		return {};

	const auto [name, value] = Split(payload, '=');
	if (!IsValidCgroupSetName(name) || !IsValidCgroupSetValue(value))
		return {};

	return {name, value};
}

inline void
TranslateParser::HandleCgroupSet(std::string_view payload)
{
	if (child_options == nullptr)
		throw std::runtime_error("misplaced CGROUP_SET packet");

	auto set = ParseCgroupSet(payload);
	if (set.first.data() == nullptr)
		throw std::runtime_error("malformed CGROUP_SET packet");

	child_options->cgroup.Set(alloc, set.first, set.second);
}

inline void
TranslateParser::HandleCgroupXattr(std::string_view payload)
{
	if (child_options == nullptr)
		throw std::runtime_error("misplaced CGROUP_XATTR packet");

	auto xattr = ParseCgroupSet(payload);
	if (xattr.first.data() == nullptr)
		throw std::runtime_error("malformed CGROUP_XATTR packet");

	child_options->cgroup.SetXattr(alloc, xattr.first, xattr.second);
}

static bool
CheckProbeSuffix(std::string_view payload) noexcept
{
	return payload.find('/') == payload.npos &&
		IsValidString(payload);
}

#if TRANSLATION_ENABLE_TRANSFORMATION

inline void
TranslateParser::HandleSubstYamlFile(std::string_view payload)
{
	const auto [prefix, rest] = Split(payload, '\0');
	if (rest.data() == nullptr)
		throw std::runtime_error("malformed SUBST_YAML_FILE packet");

	const auto [yaml_file, yaml_map_path] = Split(rest, '\0');
	if (yaml_map_path.data() == nullptr ||
	    !IsValidAbsolutePath(yaml_file))
		throw std::runtime_error("malformed SUBST_YAML_FILE packet");

	AddSubstYamlFile(prefix.data(), yaml_file.data(),
			 yaml_map_path.data());
}

#endif

inline void
TranslateParser::HandleRegularPacket(TranslationCommand command,
				     const std::span<const std::byte> payload)
{
	const std::string_view string_payload = ToStringView(payload);

	switch (command) {
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
	case TranslationCommand::LOGIN:
	case TranslationCommand::CRON:
	case TranslationCommand::PASSWORD:
	case TranslationCommand::SERVICE:
	case TranslationCommand::ALT_HOST:
	case TranslationCommand::CHAIN_HEADER:
	case TranslationCommand::AUTH_TOKEN:
	case TranslationCommand::PLAN:
		throw std::runtime_error("misplaced translate request packet");

	case TranslationCommand::UID_GID:
		HandleUidGid(payload);
		return;

	case TranslationCommand::STATUS:
		if (payload.size() != 2)
			throw std::runtime_error("size mismatch in STATUS packet from translation server");

		static_assert(sizeof(HttpStatus) == 2);
		response.status = *(const HttpStatus *)(const void *)payload.data();

#if TRANSLATION_ENABLE_HTTP
		if (!http_status_is_valid(response.status))
			throw FmtRuntimeError("invalid HTTP status code {}",
					      (unsigned)response.status);
#endif

		return;

	case TranslationCommand::PATH:
#if TRANSLATION_ENABLE_RADDRESS
		if (!IsValidAbsolutePath(string_payload))
			throw std::runtime_error("malformed PATH packet");

		if (nfs_address != nullptr && *nfs_address->path == 0) {
			nfs_address->path = string_payload.data();
			return;
		}

		if (resource_address == nullptr || resource_address->IsDefined())
			throw std::runtime_error("misplaced PATH packet");

		file_address = alloc.New<FileAddress>(string_payload.data());
		*resource_address = *file_address;
		return;
#else
		break;
#endif

	case TranslationCommand::PATH_INFO:
#if TRANSLATION_ENABLE_RADDRESS
		if (!IsValidString(string_payload))
			throw std::runtime_error("malformed PATH_INFO packet");

		if (cgi_address != nullptr &&
		    cgi_address->path_info == nullptr) {
			cgi_address->path_info = string_payload.data();
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
		if (!IsValidString(string_payload))
			throw std::runtime_error("malformed EXPAND_PATH packet");

		if (response.regex == nullptr) {
			throw std::runtime_error("misplaced EXPAND_PATH packet");
		} else if (cgi_address != nullptr && !cgi_address->expand_path) {
			cgi_address->path = string_payload.data();
			cgi_address->expand_path = true;
			return;
		} else if (nfs_address != nullptr && !nfs_address->expand_path) {
			nfs_address->path = string_payload.data();
			nfs_address->expand_path = true;
			return;
		} else if (file_address != nullptr && !file_address->expand_path) {
			file_address->path = string_payload.data();
			file_address->expand_path = true;
			return;
		} else if (http_address != NULL && !http_address->expand_path) {
			http_address->path = string_payload.data();
			http_address->expand_path = true;
			return;
		} else
			throw std::runtime_error("misplaced EXPAND_PATH packet");
#else
		break;
#endif

	case TranslationCommand::EXPAND_PATH_INFO:
#if TRANSLATION_ENABLE_RADDRESS && TRANSLATION_ENABLE_EXPAND
		if (!IsValidString(string_payload))
			throw std::runtime_error("malformed EXPAND_PATH_INFO packet");

		if (response.regex == nullptr) {
			throw std::runtime_error("misplaced EXPAND_PATH_INFO packet");
		} else if (cgi_address != nullptr &&
			   !cgi_address->expand_path_info) {
			cgi_address->path_info = string_payload.data();
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
		/* deprecated */
		return;

	case TranslationCommand::GZIPPED:
#if TRANSLATION_ENABLE_RADDRESS
		if (!IsValidAbsolutePath(string_payload))
			throw std::runtime_error("malformed GZIPPED packet");

		if (file_address != nullptr) {
			if (file_address->auto_gzipped ||
			    file_address->gzipped != nullptr)
				throw std::runtime_error("duplicate GZIPPED packet");

			file_address->gzipped = string_payload.data();
			return;
		} else if (nfs_address != nullptr) {
			/* ignore for now */
			return;
		} else {
			throw std::runtime_error("misplaced GZIPPED packet");
		}
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
			response.site = string_payload.data();
#if TRANSLATION_ENABLE_RADDRESS
		else
			throw std::runtime_error("misplaced SITE packet");
#endif

		return;

	case TranslationCommand::CONTENT_TYPE:
#if TRANSLATION_ENABLE_RADDRESS
		if (!IsValidNonEmptyString(string_payload))
			throw std::runtime_error("malformed CONTENT_TYPE packet");

		if (file_address != nullptr) {
			if (file_address->content_type_lookup.data() != nullptr)
				throw std::runtime_error("CONTENT_TYPE/CONTENT_TYPE_LOOKUP conflict");

			file_address->content_type = string_payload.data();
		} else if (nfs_address != nullptr) {
			if (nfs_address->content_type_lookup.data() != nullptr)
				throw std::runtime_error("CONTENT_TYPE/CONTENT_TYPE_LOOKUP conflict");

			nfs_address->content_type = string_payload.data();
		} else if (from_request.content_type_lookup) {
			response.content_type = string_payload.data();
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

		http_address = http_address_parse(alloc, string_payload.data());

		*resource_address = *http_address;

		FinishAddressList();
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

		response.redirect = string_payload.data();
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

		response.redirect = string_payload.data();
		response.expand_redirect = true;
		return;
#else
		break;
#endif

	case TranslationCommand::BOUNCE:
#if TRANSLATION_ENABLE_HTTP
		if (!IsValidNonEmptyString(string_payload))
			throw std::runtime_error("malformed BOUNCE packet");

		response.bounce = string_payload.data();
		return;
#else
		break;
#endif

	case TranslationCommand::FILTER:
#if TRANSLATION_ENABLE_TRANSFORMATION
		FinishAddressList();

		resource_address = AddFilter();
		child_options = nullptr;
		ns_options = nullptr;
		file_address = nullptr;
		cgi_address = nullptr;
		nfs_address = nullptr;
		lhttp_address = nullptr;
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

	case TranslationCommand::PROCESS: {
#if TRANSLATION_ENABLE_TRANSFORMATION
		auto *new_transformation = AddTransformation(XmlProcessorTransformation{});
		new_transformation->u.processor.options = PROCESSOR_REWRITE_URL;
		return;
#else
		break;
#endif
	}

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
		response.container_groups.Add(alloc, string_payload.data());
		return;
#else
		break;
#endif

	case TranslationCommand::WIDGET_GROUP:
#if TRANSLATION_ENABLE_WIDGET
		if (!IsValidNonEmptyString(string_payload))
			throw std::runtime_error("malformed WIDGET_GROUP packet");

		response.widget_group = string_payload.data();
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

		response.untrusted = string_payload.data();
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

		response.untrusted_prefix = string_payload.data();
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

		response.untrusted_site_suffix = string_payload.data();
		return;
#else
		break;
#endif

	case TranslationCommand::SCHEME:
#if TRANSLATION_ENABLE_HTTP
		if (!string_payload.starts_with("http"sv))
			throw std::runtime_error("malformed SCHEME packet");

		response.scheme = string_payload.data();
		return;
#else
		break;
#endif

	case TranslationCommand::HOST:
#if TRANSLATION_ENABLE_HTTP
		response.host = string_payload.data();
		return;
#else
		break;
#endif

	case TranslationCommand::URI:
#if TRANSLATION_ENABLE_HTTP
		if (!IsValidAbsoluteUriPath(string_payload))
			throw std::runtime_error("malformed URI packet");

		response.uri = string_payload.data();
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
		response.user = string_payload.data();
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

		response.realm = string_payload.data();
		return;
#else
		break;
#endif

	case TranslationCommand::LANGUAGE:
#if TRANSLATION_ENABLE_SESSION
		response.language = string_payload.data();
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

		SetCgiAddress(ResourceAddress::Type::PIPE, string_payload.data());
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

		SetCgiAddress(ResourceAddress::Type::CGI, string_payload.data());
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

		SetCgiAddress(ResourceAddress::Type::FASTCGI, string_payload.data());
		FinishAddressList();
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

		nfs_address = alloc.New<NfsAddress>(string_payload.data(), "", "");
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

		nfs_address->export_name = string_payload.data();
		return;
#else
		break;
#endif

	case TranslationCommand::JAILCGI:
		/* obsolete */
		break;

	case TranslationCommand::HOME:
		translate_client_home(ns_options,
				      string_payload);
		return;

	case TranslationCommand::INTERPRETER:
#if TRANSLATION_ENABLE_RADDRESS
		if (resource_address == nullptr ||
		    (resource_address->type != ResourceAddress::Type::CGI &&
		     resource_address->type != ResourceAddress::Type::FASTCGI) ||
		    cgi_address->interpreter != nullptr)
			throw std::runtime_error("misplaced INTERPRETER packet");

		cgi_address->interpreter = string_payload.data();
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

		cgi_address->action = string_payload.data();
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

		cgi_address->script_name = string_payload.data();
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

		cgi_address->script_name = string_payload.data();
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
			cgi_address->document_root = string_payload.data();
		else if (file_address != nullptr &&
			 file_address->delegate != nullptr)
			file_address->document_root = string_payload.data();
		else
			response.document_root = string_payload.data();
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
			cgi_address->document_root = string_payload.data();
			cgi_address->expand_document_root = true;
		} else if (file_address != nullptr &&
			   file_address->delegate != nullptr) {
			file_address->document_root = string_payload.data();
			file_address->expand_document_root = true;
		} else {
			response.document_root = string_payload.data();
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

		if (payload.size() < 2)
			throw std::runtime_error("malformed ADDRESS packet");

		address_list_builder.Add(alloc, SocketAddress{payload});
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
			address_list_builder.Add(alloc,
						 ParseSocketAddress(string_payload.data(),
								    default_port, false));
		} catch (const std::exception &e) {
			throw FmtRuntimeError("malformed ADDRESS_STRING packet: {}",
					      e.what());
		}

		return;
#else
		break;
#endif

	case TranslationCommand::VIEW:
#if TRANSLATION_ENABLE_WIDGET
		if (!valid_view_name(string_payload.data()))
			throw std::runtime_error("invalid view name");

		AddView(string_payload.data());
		return;
#else
		break;
#endif

	case TranslationCommand::MAX_AGE:
		if (payload.size() != 4)
			throw std::runtime_error("malformed MAX_AGE packet");

		switch (previous_command) {
		case TranslationCommand::BEGIN:
			response.max_age = std::chrono::seconds(*(const uint32_t *)(const void *)payload.data());
			break;

#if TRANSLATION_ENABLE_SESSION
		case TranslationCommand::USER:
			response.user_max_age = std::chrono::seconds(*(const uint32_t *)(const void *)payload.data());
			break;
#endif

		default:
			throw std::runtime_error("misplaced MAX_AGE packet");
		}

		return;

	case TranslationCommand::VARY:
#if TRANSLATION_ENABLE_CACHE
		if (payload.empty() ||
		    payload.size() % sizeof(response.vary.front()) != 0)
			throw std::runtime_error("malformed VARY packet");

		response.vary = FromBytesFloor<const TranslationCommand>(payload);
#endif
		return;

	case TranslationCommand::INVALIDATE:
#if TRANSLATION_ENABLE_CACHE
		if (payload.empty() ||
		    payload.size() % sizeof(response.invalidate.front()) != 0)
			throw std::runtime_error("malformed INVALIDATE packet");

		response.invalidate = {
			(const TranslationCommand *)(const void *)payload.data(),
			payload.size() / sizeof(response.invalidate.front()),
		};
#endif
		return;

	case TranslationCommand::BASE:
#if TRANSLATION_ENABLE_RADDRESS
		if (!IsValidAbsoluteUriPath(string_payload) ||
		    string_payload.back() != '/')
			throw std::runtime_error("malformed BASE packet");

		if (response.layout.data() != nullptr) {
			assert(layout_items_builder);

			layout_items_builder->emplace_back(TranslationLayoutItem::Base{},
							   string_payload);
			return;
		}

		if (from_request.uri == nullptr ||
		    response.auto_base ||
		    response.base != nullptr)
			throw std::runtime_error("misplaced BASE packet");

		base_suffix = base_tail(from_request.uri, string_payload);
		if (base_suffix == nullptr)
			throw std::runtime_error("BASE mismatches request URI");

		response.base = string_payload.data();
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

		if (response.unsafe_base)
			throw std::runtime_error("duplicate UNSAFE_BASE");

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
		if (!IsValidNonEmptyString(string_payload))
			throw std::runtime_error("malformed REGEX packet");

		if (response.layout.data() != nullptr) {
			assert(layout_items_builder);

			layout_items_builder->emplace_back(TranslationLayoutItem::Regex{},
							   string_payload);
			return;
		}

		if (response.base == nullptr)
			throw std::runtime_error("REGEX without BASE");

		if (response.regex != nullptr)
			throw std::runtime_error("duplicate REGEX");

		response.regex = string_payload.data();
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

		response.inverse_regex = string_payload.data();
		return;
#else
		break;
#endif

	case TranslationCommand::REGEX_TAIL:
#if TRANSLATION_ENABLE_EXPAND
		if (!payload.empty())
			throw std::runtime_error("malformed REGEX_TAIL packet");

		if (response.regex == nullptr &&
		    response.inverse_regex == nullptr &&
		    response.layout.data() == nullptr)
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

		file_address->delegate = alloc.New<DelegateAddress>(string_payload.data());
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

		args_builder.Add(alloc, string_payload.data(), false);
		return;

	case TranslationCommand::EXPAND_APPEND:
#if TRANSLATION_ENABLE_EXPAND
		if (!IsValidNonEmptyString(string_payload))
			throw std::runtime_error("malformed EXPAND_APPEND packet");

		if (response.regex == nullptr || !HasArgs() ||
		    !args_builder.CanSetExpand())
			throw std::runtime_error("misplaced EXPAND_APPEND packet");

		args_builder.SetExpand(string_payload.data());
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

	case TranslationCommand::DISCARD_REALM_SESSION:
#if TRANSLATION_ENABLE_SESSION
		response.discard_realm_session = true;
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

		response.www_authenticate = string_payload.data();
		return;
#else
		break;
#endif

	case TranslationCommand::AUTHENTICATION_INFO:
#if TRANSLATION_ENABLE_SESSION
		if (!IsValidNonEmptyString(string_payload))
			throw std::runtime_error("malformed AUTHENTICATION_INFO packet");

		response.authentication_info = string_payload.data();
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

	case TranslationCommand::REQUIRE_CSRF_TOKEN:
#if TRANSLATION_ENABLE_SESSION
		response.require_csrf_token = true;
		return;
#else
		break;
#endif

	case TranslationCommand::SEND_CSRF_TOKEN:
#if TRANSLATION_ENABLE_SESSION
		response.send_csrf_token = true;
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

		response.cookie_domain = string_payload.data();
		return;
#else
		break;
#endif

	case TranslationCommand::ERROR_DOCUMENT:
		response.error_document = payload;
		return;

	case TranslationCommand::CHECK:
#if TRANSLATION_ENABLE_SESSION
		if (response.check.data() != nullptr)
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

		SetCgiAddress(ResourceAddress::Type::WAS, string_payload.data());
		FinishAddressList();
		address_list = &cgi_address->address_list;
		default_port = 0;
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

		address_list_builder.SetStickyMode(StickyMode::SESSION_MODULO);
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

		response.cookie_host = string_payload.data();
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

		response.cookie_path = string_payload.data();
		return;
#else
		break;
#endif

	case TranslationCommand::PROCESS_CSS: {
#if TRANSLATION_ENABLE_TRANSFORMATION
		auto *new_transformation = AddTransformation(CssProcessorTransformation{});
		new_transformation->u.css_processor.options = CSS_PROCESSOR_REWRITE_URL;
		return;
#else
		break;
#endif
	}

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
		AddTransformation(TextProcessorTransformation{});
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

		response.local_uri = string_payload.data();
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
		if (string_payload.size() < 10 || string_payload[8] != '/' ||
		    memchr(string_payload.data() + 9, 0,
			   string_payload.size() - 9) != nullptr)
			throw std::runtime_error("malformed VALIDATE_MTIME packet");

		response.validate_mtime.mtime = *(const uint64_t *)(const void *)payload.data();
		response.validate_mtime.path =
			alloc.DupZ(string_payload.substr(8));
		return;

	case TranslationCommand::LHTTP_PATH:
#if TRANSLATION_ENABLE_RADDRESS
		if (resource_address == nullptr || resource_address->IsDefined())
			throw std::runtime_error("misplaced LHTTP_PATH packet");

		if (!IsValidAbsolutePath(string_payload))
			throw std::runtime_error("malformed LHTTP_PATH packet");

		lhttp_address = alloc.New<LhttpAddress>(string_payload.data());
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

		lhttp_address->uri = string_payload.data();
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

		lhttp_address->uri = string_payload.data();
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

		lhttp_address->host_and_port = string_payload.data();
		return;
#else
		break;
#endif

	case TranslationCommand::CONCURRENCY:
#if TRANSLATION_ENABLE_RADDRESS
		if (payload.size() != 2)
			throw std::runtime_error("malformed CONCURRENCY packet");

		if (lhttp_address != nullptr)
			lhttp_address->concurrency = *(const uint16_t *)(const void *)payload.data();
		else if (cgi_address != nullptr)
			cgi_address->concurrency = *(const uint16_t *)(const void *)payload.data();
		else
			throw std::runtime_error("misplaced CONCURRENCY packet");

		return;
#else
		break;
#endif

	case TranslationCommand::WANT_FULL_URI:
#if TRANSLATION_ENABLE_HTTP
		if (from_request.want_full_uri)
			throw std::runtime_error("WANT_FULL_URI loop");

		if (response.want_full_uri.data() != nullptr)
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
		translate_client_mount_proc(ns_options, payload.size());
		return;

	case TranslationCommand::MOUNT_HOME:
		HandleMountHome(string_payload);
		return;

	case TranslationCommand::BIND_MOUNT:
		previous_command = command;
		HandleBindMount(string_payload, false, false);
		return;

	case TranslationCommand::MOUNT_TMP_TMPFS:
		translate_client_mount_tmp_tmpfs(ns_options, string_payload);
		return;

	case TranslationCommand::UTS_NAMESPACE:
		translate_client_uts_namespace(ns_options, string_payload);
		return;

	case TranslationCommand::RLIMITS:
		translate_client_rlimits(alloc, child_options, string_payload.data());
		return;

	case TranslationCommand::WANT:
#if TRANSLATION_ENABLE_WANT
		HandleWant((const TranslationCommand *)(const void *)payload.data(),
			   payload.size());
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

		response.test_path = string_payload.data();
		return;

	case TranslationCommand::EXPAND_TEST_PATH:
#if TRANSLATION_ENABLE_EXPAND
		if (response.regex == nullptr)
			throw std::runtime_error("misplaced EXPAND_TEST_PATH packet");

		if (!IsValidNonEmptyString(string_payload))
			throw std::runtime_error("malformed EXPAND_TEST_PATH packet");

		if (response.expand_test_path)
			throw std::runtime_error("duplicate EXPAND_TEST_PATH packet");

		response.test_path = string_payload.data();
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
					     string_payload,
					     false);
		return;

	case TranslationCommand::AUTH:
#if TRANSLATION_ENABLE_SESSION
		if (response.HasAuth())
			throw std::runtime_error("duplicate AUTH packet");

		if (response.http_auth.data() != nullptr)
			throw std::runtime_error("cannot combine AUTH and HTTP_AUTH");

		if (response.token_auth.data() != nullptr)
			throw std::runtime_error("cannot combine AUTH and TOKEN_AUTH");

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

		response.uri = string_payload.data();
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

		response.site = string_payload.data();
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
#if TRANSLATION_ENABLE_RADDRESS
		if (!payload.empty())
			throw std::runtime_error("malformed AUTO_GZIPPED packet");

		if (file_address != nullptr) {
			if (file_address->auto_gzipped ||
			    file_address->gzipped != nullptr)
				throw std::runtime_error("duplicate AUTO_GZIPPED packet");

			file_address->auto_gzipped = true;
		} else if (nfs_address != nullptr) {
			/* ignore for now */
		} else if (from_request.content_type_lookup) {
			if (response.auto_gzipped)
				throw std::runtime_error("duplicate AUTO_GZIPPED packet");

			response.auto_gzipped = true;
		} else
			throw std::runtime_error("misplaced AUTO_GZIPPED packet");
#endif
		return;

	case TranslationCommand::PROBE_PATH_SUFFIXES:
		if (response.probe_path_suffixes.data() != nullptr ||
		    response.test_path == nullptr)
			throw std::runtime_error("misplaced PROBE_PATH_SUFFIXES packet");

		response.probe_path_suffixes = payload;
		return;

	case TranslationCommand::PROBE_SUFFIX:
		if (response.probe_path_suffixes.data() == nullptr)
			throw std::runtime_error("misplaced PROBE_SUFFIX packet");

		if (probe_suffixes_builder.full())
			throw std::runtime_error("too many PROBE_SUFFIX packets");

		if (!CheckProbeSuffix(string_payload))
			throw std::runtime_error("malformed PROBE_SUFFIX packets");

		probe_suffixes_builder.push_back(string_payload.data());
		return;

	case TranslationCommand::AUTH_FILE:
#if TRANSLATION_ENABLE_SESSION
		if (response.HasAuth())
			throw std::runtime_error("duplicate AUTH_FILE packet");

		if (!IsValidAbsolutePath(string_payload))
			throw std::runtime_error("malformed AUTH_FILE packet");

		response.auth_file = string_payload.data();
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

		response.auth_file = string_payload.data();
		response.expand_auth_file = true;
		return;
#else
		break;
#endif

	case TranslationCommand::APPEND_AUTH:
#if TRANSLATION_ENABLE_SESSION
		if (!response.HasAuth() ||
		    response.append_auth.data() != nullptr ||
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
		    response.append_auth.data() != nullptr ||
		    response.expand_append_auth != nullptr)
			throw std::runtime_error("misplaced EXPAND_APPEND_AUTH packet");

		if (!IsValidNonEmptyString(string_payload))
			throw std::runtime_error("malformed EXPAND_APPEND_AUTH packet");

		response.expand_append_auth = string_payload.data();
		return;
#else
		break;
#endif

	case TranslationCommand::LISTENER_TAG:
		if (!IsValidNonEmptyString(string_payload))
			throw std::runtime_error("malformed LISTENER_TAG packet");

		if (response.listener_tag != nullptr)
			throw std::runtime_error("duplicate LISTENER_TAG packet");

		response.listener_tag = string_payload.data();
		return;

	case TranslationCommand::EXPAND_COOKIE_HOST:
#if TRANSLATION_ENABLE_SESSION
		if (response.regex == nullptr ||
		    resource_address == nullptr ||
		    !resource_address->IsDefined())
			throw std::runtime_error("misplaced EXPAND_COOKIE_HOST packet");

		if (!IsValidNonEmptyString(string_payload))
			throw std::runtime_error("malformed EXPAND_COOKIE_HOST packet");

		response.cookie_host = string_payload.data();
		response.expand_cookie_host = true;
		return;
#else
		break;
#endif

	case TranslationCommand::EXPAND_BIND_MOUNT:
#if TRANSLATION_ENABLE_EXPAND
		previous_command = command;
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

		response.read_file = string_payload.data();
		return;

	case TranslationCommand::EXPAND_READ_FILE:
#if TRANSLATION_ENABLE_EXPAND
		if (response.read_file != nullptr)
			throw std::runtime_error("duplicate EXPAND_READ_FILE packet");

		if (!IsValidNonEmptyString(string_payload))
			throw std::runtime_error("malformed EXPAND_READ_FILE packet");

		response.read_file = string_payload.data();
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
		response.session_site = string_payload.data();
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
		/* deprecated */
		return;

	case TranslationCommand::EXPAND_HOME:
#if TRANSLATION_ENABLE_EXPAND
		translate_client_expand_home(ns_options,
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
#if TRANSLATION_ENABLE_RADDRESS
		if (!payload.empty())
			throw std::runtime_error("malformed AUTO_GZIP packet");

		if (response.auto_gzip)
			throw std::runtime_error("misplaced AUTO_GZIP packet");

		response.auto_gzip = true;
		return;
#else
		break;
#endif

	case TranslationCommand::INTERNAL_REDIRECT:
#if TRANSLATION_ENABLE_HTTP
		if (response.internal_redirect.data() != nullptr)
			throw std::runtime_error("duplicate INTERNAL_REDIRECT packet");

		response.internal_redirect = payload;
		return;
#else
		break;
#endif

	case TranslationCommand::HTTP_AUTH:
#if TRANSLATION_ENABLE_HTTP
		if (response.http_auth.data() != nullptr)
			throw std::runtime_error("duplicate HTTP_AUTH packet");

		if (response.auth.data() != nullptr)
			throw std::runtime_error("cannot combine AUTH and HTTP_AUTH");

		response.http_auth = payload;
		return;
#else
		break;
#endif

	case TranslationCommand::TOKEN_AUTH:
#if TRANSLATION_ENABLE_HTTP
		if (response.token_auth.data() != nullptr)
			throw std::runtime_error("duplicate TOKEN_AUTH packet");

		if (response.auth.data() != nullptr)
			throw std::runtime_error("cannot combine AUTH and TOKEN_AUTH");

		response.token_auth = payload;
		return;
#else
		break;
#endif

	case TranslationCommand::REFENCE:
		/* obsolete */
		break;

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
		previous_command = command;
		HandleBindMount(string_payload, false, true);
		return;

	case TranslationCommand::EXPAND_BIND_MOUNT_RW:
#if TRANSLATION_ENABLE_EXPAND
		previous_command = command;
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

		response.untrusted_raw_site_suffix = string_payload.data();
		return;
#else
		break;
#endif

	case TranslationCommand::MOUNT_TMPFS:
		HandleMountTmpfs(string_payload, true);
		return;

	case TranslationCommand::MOUNT_EMPTY:
		HandleMountTmpfs(string_payload, false);
		return;

	case TranslationCommand::REVEAL_USER:
#if TRANSLATION_ENABLE_TRANSFORMATION
		if (!payload.empty())
			throw std::runtime_error("malformed REVEAL_USER packet");

		if (filter == nullptr || filter->reveal_user)
			throw std::runtime_error("misplaced REVEAL_USER packet");

		filter->reveal_user = true;
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

		if (!valid_view_name(string_payload.data()))
			throw std::runtime_error("malformed CGROUP packet");

		child_options->cgroup.name = string_payload.data();
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
			http_address_parse(alloc, string_payload.data());

		FinishAddressList();
		address_list = &http_address->addresses;
		default_port = http_address->GetDefaultPort();
		return;
#else
		break;
#endif

	case TranslationCommand::EXTERNAL_SESSION_KEEPALIVE: {
#if TRANSLATION_ENABLE_SESSION
		const uint16_t *value = (const uint16_t *)(const void *)payload.data();
		if (payload.size() != sizeof(*value) || *value == 0)
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
		previous_command = command;
		HandleBindMount(string_payload, false, false, true);
		return;

	case TranslationCommand::EXPAND_BIND_MOUNT_EXEC:
#if TRANSLATION_ENABLE_EXPAND
		previous_command = command;
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

		response.execute = string_payload.data();
		args_builder = response.args;
		return;
#else
		break;
#endif

	case TranslationCommand::POOL:
		if (!IsValidNonEmptyString(string_payload))
			throw std::runtime_error("malformed POOL packet");

		response.pool = string_payload.data();
		return;

	case TranslationCommand::MESSAGE:
		if (string_payload.size() > 1024 ||
		    !IsValidNonEmptyString(string_payload))
			throw std::runtime_error("malformed MESSAGE packet");

		response.message = string_payload.data();
		return;

	case TranslationCommand::CANONICAL_HOST:
		if (!IsValidNonEmptyString(string_payload))
			throw std::runtime_error("malformed CANONICAL_HOST packet");

		response.canonical_host = string_payload.data();
		return;

	case TranslationCommand::SHELL:
#if TRANSLATION_ENABLE_EXECUTE
		if (!IsValidAbsolutePath(string_payload))
			throw std::runtime_error("malformed SHELL packet");

		if (response.shell != nullptr)
			throw std::runtime_error("duplicate SHELL packet");

		response.shell = string_payload.data();
		return;
#else
		break;
#endif

	case TranslationCommand::TOKEN:
		if (!IsValidString(string_payload))
			throw std::runtime_error("malformed TOKEN packet");
		response.token = string_payload.data();
		return;

	case TranslationCommand::STDERR_PATH_JAILED:
		translate_client_stderr_path(child_options, string_payload, true);
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

		if (payload.size() == sizeof(response.https_only)) {
			response.https_only = *(const uint16_t *)(const void *)payload.data();
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

		ns_options->network_namespace = string_payload.data();
		return;

	case TranslationCommand::MOUNT_ROOT_TMPFS:
		translate_client_mount_root_tmpfs(ns_options, payload.size());
		return;

	case TranslationCommand::CHILD_TAG:
		if (!IsValidNonEmptyString(string_payload))
			throw std::runtime_error("malformed CHILD_TAG packet");

		if (child_options == nullptr)
			throw std::runtime_error("misplaced CHILD_TAG packet");

		if (child_options->tag.data() == nullptr)
			child_options->tag = string_payload;
		else
			child_options->tag =
				alloc.ConcatView(child_options->tag, '\0',
						 string_payload);

		return;

	case TranslationCommand::CERTIFICATE:
#if TRANSLATION_ENABLE_RADDRESS
		if (http_address == nullptr || !http_address->ssl)
			throw std::runtime_error("misplaced CERTIFICATE packet");

		if (http_address->certificate != nullptr)
			throw std::runtime_error("duplicate CERTIFICATE packet");

		if (!IsValidName(string_payload))
			throw std::runtime_error("malformed CERTIFICATE packet");

		http_address->certificate = string_payload.data();
		return;
#else
		break;
#endif

	case TranslationCommand::UNCACHED:
#if TRANSLATION_ENABLE_CACHE
#if TRANSLATION_ENABLE_RADDRESS
		if (resource_address == nullptr)
			throw std::runtime_error("misplaced UNCACHED packet");
#endif

		if (response.uncached)
			throw std::runtime_error("duplicate UNCACHED packet");

		response.uncached = true;
		return;
#else // !TRANSLATION_ENABLE_CACHE
		break;
#endif

	case TranslationCommand::PID_NAMESPACE_NAME:
		if (!IsValidName(string_payload))
			throw std::runtime_error("malformed PID_NAMESPACE_NAME packet");

		if (ns_options == nullptr)
			throw std::runtime_error("misplaced PID_NAMESPACE_NAME packet");

		if (ns_options->pid_namespace != nullptr)
			throw std::runtime_error("duplicate PID_NAMESPACE_NAME packet");

		if (ns_options->enable_pid)
			throw std::runtime_error("Can't combine PID_NAMESPACE_NAME with PID_NAMESPACE");

		ns_options->pid_namespace = string_payload.data();
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

	case TranslationCommand::CACHE_TAG:
#if TRANSLATION_ENABLE_CACHE
		if (!IsValidNonEmptyString(string_payload))
			throw std::runtime_error("malformed CACHE_TAG packet");

#if TRANSLATION_ENABLE_TRANSFORMATION
		if (filter != nullptr) {
			if (filter->cache_tag != nullptr)
				throw std::runtime_error("duplicate CACHE_TAG packet");

			filter->cache_tag = string_payload.data();
			return;
		}
#endif // TRANSLATION_ENABLE_TRANSFORMATION

		if (!response.address.IsDefined())
			throw std::runtime_error("misplaced CACHE_TAG packet");

		if (response.cache_tag != nullptr)
			throw std::runtime_error("duplicate CACHE_TAG packet");

		response.cache_tag = string_payload.data();
		return;
#else
		break;
#endif // TRANSLATION_ENABLE_CACHE

	case TranslationCommand::HTTP2:
#if TRANSLATION_ENABLE_HTTP
		if (http_address == nullptr)
			throw std::runtime_error("misplaced HTTP2 packet");

		if (http_address->http2)
			throw std::runtime_error("duplicate HTTP2 packet");

		if (!payload.empty())
			throw std::runtime_error("malformed HTTP2 packet");

		http_address->http2 = true;
		return;
#else
		break;
#endif

	case TranslationCommand::REQUEST_URI_VERBATIM:
#if TRANSLATION_ENABLE_RADDRESS
		if (cgi_address == nullptr)
			throw std::runtime_error("misplaced REQUEST_URI_VERBATIM packet");

		if (!payload.empty())
			throw std::runtime_error("malformed REQUEST_URI_VERBATIM packet");

		if (cgi_address->request_uri_verbatim)
			throw std::runtime_error("duplicate REQUEST_URI_VERBATIM packet");

		cgi_address->request_uri_verbatim = true;
		return;
#else
		break;
#endif

	case TranslationCommand::DEFER:
		if (!payload.empty())
			throw std::runtime_error("malformed DEFER packet");

		response.defer = true;
		return;

	case TranslationCommand::STDERR_POND:
		if (!payload.empty())
			throw std::runtime_error("malformed STDERR_POND packet");

		if (child_options == nullptr)
			throw std::runtime_error("misplaced STDERR_POND packet");

		if (child_options->stderr_pond)
			throw std::runtime_error("duplicate STDERR_POND packet");

		child_options->stderr_pond = true;
		return;

	case TranslationCommand::CHAIN:
#if TRANSLATION_ENABLE_HTTP
		if (response.chain.data() != nullptr)
			throw std::runtime_error("duplicate CHAIN packet");

		response.chain = payload;
		return;
#else
		break;
#endif

	case TranslationCommand::BREAK_CHAIN:
#if TRANSLATION_ENABLE_HTTP
		if (!payload.empty())
			throw std::runtime_error("malformed BREAK_CHAIN packet");

		if (!from_request.chain)
			throw std::runtime_error("BREAK_CHAIN without CHAIN request");

		if (response.break_chain)
			throw std::runtime_error("duplicate BREAK_CHAIN packet");

		response.break_chain = true;
		return;
#else
		break;
#endif

	case TranslationCommand::FILTER_NO_BODY:
#if TRANSLATION_ENABLE_TRANSFORMATION
		if (!payload.empty())
			throw std::runtime_error("malformed FILTER_NO_BODY packet");

		if (filter == nullptr)
			throw std::runtime_error("misplaced FILTER_NO_BODY");

		if (filter->no_body)
			throw std::runtime_error("duplicate FILTER_NO_BODY");

		filter->no_body = true;
		return;
#else
		break;
#endif

	case TranslationCommand::TINY_IMAGE:
#if TRANSLATION_ENABLE_HTTP
		if (!payload.empty())
			throw std::runtime_error("malformed TINY_IMAGE packet");

		if (response.tiny_image)
			throw std::runtime_error("duplicate TINY_IMAGE packet");

		response.tiny_image = true;
		return;
#else
		break;
#endif

	case TranslationCommand::ATTACH_SESSION:
#if TRANSLATION_ENABLE_SESSION
		if (payload.empty())
			throw std::runtime_error("malformed ATTACH_SESSION packet");

		if (response.attach_session.data() != nullptr)
			throw std::runtime_error("duplicate ATTACH_SESSION packet");

		response.attach_session = payload;
		return;
#else
		break;
#endif

	case TranslationCommand::LIKE_HOST:
		if (!IsValidNonEmptyString(string_payload))
			throw std::runtime_error("malformed LIKE_HOST packet");

		if (response.like_host != nullptr)
			throw std::runtime_error("duplicate LIKE_HOST packet");

		response.like_host = string_payload.data();
		return;

	case TranslationCommand::LAYOUT:
#if TRANSLATION_ENABLE_RADDRESS
		if (payload.empty())
			throw std::runtime_error("malformed LAYOUT packet");

		if (response.layout.data() != nullptr)
			throw std::runtime_error("duplicate LAYOUT packet");

		response.layout = payload;
		layout_items_builder = std::make_shared<std::vector<TranslationLayoutItem>>();
		return;
#else
		break;
#endif

	case TranslationCommand::RECOVER_SESSION:
#if TRANSLATION_ENABLE_SESSION
		if (response.recover_session != nullptr)
			throw std::runtime_error("duplicate RECOVER_SESSION packet");

		if (string_payload.empty() ||
		    !IsValidCookieValue(string_payload))
			throw std::runtime_error("malformed RECOVER_SESSION packet");

		response.recover_session = string_payload.data();
		return;
#else
		break;
#endif

	case TranslationCommand::OPTIONAL:
		if (!payload.empty())
			throw std::runtime_error("malformed OPTIONAL packet");

		switch (previous_command) {
		case TranslationCommand::BIND_MOUNT:
		case TranslationCommand::EXPAND_BIND_MOUNT:
		case TranslationCommand::BIND_MOUNT_RW:
		case TranslationCommand::EXPAND_BIND_MOUNT_RW:
		case TranslationCommand::BIND_MOUNT_EXEC:
		case TranslationCommand::EXPAND_BIND_MOUNT_EXEC:
		case TranslationCommand::BIND_MOUNT_FILE:
		case TranslationCommand::WRITE_FILE:
			if (ns_options != nullptr &&
			    mount_list != ns_options->mount.mounts.before_begin()) {
				mount_list->optional = true;
				return;
			}

			break;

		default:
			break;
		}

		throw std::runtime_error("misplaced OPTIONAL packet");

	case TranslationCommand::AUTO_BROTLI_PATH:
#if TRANSLATION_ENABLE_RADDRESS
		if (!payload.empty())
			throw std::runtime_error("malformed AUTO_BROTLI_PATH packet");

		if (file_address != nullptr) {
			if (file_address->auto_brotli_path)
				throw std::runtime_error("duplicate AUTO_BROTLI_PATH packet");

			file_address->auto_brotli_path = true;
		} else if (nfs_address != nullptr) {
			/* ignore for now */
		} else if (from_request.content_type_lookup) {
			if (response.auto_brotli_path)
				throw std::runtime_error("duplicate AUTO_BROTLI_PATH packet");

			response.auto_brotli_path = true;
		} else
			throw std::runtime_error("misplaced AUTO_BROTLI_PATH packet");
#endif
		return;

	case TranslationCommand::TRANSPARENT_CHAIN:
#if TRANSLATION_ENABLE_HTTP
		if (!payload.empty())
			throw std::runtime_error("malformed TRANSPARENT_CHAIN packet");

		if (response.chain.data() == nullptr)
			throw std::runtime_error("TRANSPARENT_CHAIN without CHAIN");

		if (response.transparent_chain)
			throw std::runtime_error("duplicate TRANSPARENT_CHAIN packet");

		response.transparent_chain = true;
		return;
#else
		break;
#endif

	case TranslationCommand::STATS_TAG:
		if (!IsValidNonEmptyString(string_payload))
			throw std::runtime_error("malformed STATS_TAG packet");

		if (response.stats_tag != nullptr)
			throw std::runtime_error("duplicate STATS_TAG packet");

		response.stats_tag = string_payload.data();
		return;

	case TranslationCommand::MOUNT_DEV:
		if (!payload.empty())
			throw std::runtime_error("malformed MOUNT_DEV packet");

		if (ns_options == nullptr ||
		    (!ns_options->mount.mount_root_tmpfs &&
		     ns_options->mount.pivot_root == nullptr))
			throw std::runtime_error("misplaced MOUNT_DEV packet");

		if (ns_options->mount.mount_dev)
			throw std::runtime_error("duplicate MOUNT_DEV packet");

		ns_options->mount.mount_dev = true;
		return;

	case TranslationCommand::BIND_MOUNT_FILE:
		previous_command = command;
		HandleBindMount(string_payload, false, false, false, true);
		return;

	case TranslationCommand::EAGER_CACHE:
#if TRANSLATION_ENABLE_CACHE
#if TRANSLATION_ENABLE_RADDRESS
		if (resource_address == nullptr)
			throw std::runtime_error("misplaced EAGER_CACHE packet");
#endif

		if (response.eager_cache)
			throw std::runtime_error("duplicate EAGER_CACHE packet");

		response.eager_cache = true;
		return;
#else // !TRANSLATION_ENABLE_CACHE
		break;
#endif

	case TranslationCommand::AUTO_FLUSH_CACHE:
#if TRANSLATION_ENABLE_CACHE
#if TRANSLATION_ENABLE_RADDRESS
		if (resource_address == nullptr)
			throw std::runtime_error("misplaced AUTO_FLUSH_CACHE packet");
#endif

		if (response.cache_tag == nullptr)
			throw std::runtime_error("AUTO_FLUSH_CACHE without CACHE_TAG");

		if (response.auto_flush_cache)
			throw std::runtime_error("duplicate AUTO_FLUSH_CACHE packet");

		response.auto_flush_cache = true;
		return;
#else // !TRANSLATION_ENABLE_CACHE
		break;
#endif

	case TranslationCommand::PARALLELISM:
#if TRANSLATION_ENABLE_RADDRESS
		if (payload.size() != 2)
			throw std::runtime_error("malformed CONCURRENCY packet");

		if (lhttp_address != nullptr)
			lhttp_address->parallelism = *(const uint16_t *)(const void *)payload.data();
		else if (cgi_address != nullptr)
			cgi_address->parallelism = *(const uint16_t *)(const void *)payload.data();
		else
			throw std::runtime_error("misplaced CONCURRENCY packet");
		return;
#else
		break;
#endif

	case TranslationCommand::EXPIRES_RELATIVE_WITH_QUERY:
		translate_client_expires_relative_with_query(response, payload);
		return;

	case TranslationCommand::CGROUP_XATTR:
		HandleCgroupXattr(string_payload);
		return;

	case TranslationCommand::CHECK_HEADER:
#if TRANSLATION_ENABLE_SESSION
		if (response.check.data() == nullptr)
			throw std::runtime_error("CHECK_HEADER without CHECK");

		if (response.check_header != nullptr)
			throw std::runtime_error("duplicate CHECK_HEADER packet");

		if (!IsValidLowerHeaderName(string_payload))
			throw std::runtime_error("malformed CHECK_HEADER packet");

		response.check_header = string_payload.data();
		return;
#else
		break;
#endif

	case TranslationCommand::CHDIR:
		if (child_options == nullptr)
			throw std::runtime_error("misplaced CHDIR packet");

		if (!IsValidAbsolutePath(string_payload))
			throw std::runtime_error("malformed CHDIR packet");

		child_options->chdir = string_payload.data();
		return;

	case TranslationCommand::SESSION_COOKIE_SAME_SITE:
#if TRANSLATION_ENABLE_SESSION
		response.session_cookie_same_site = ParseCookieSameSite(string_payload);
		return;
#else
		break;
#endif

	case TranslationCommand::NO_PASSWORD:
#if TRANSLATION_ENABLE_LOGIN
		if (response.no_password != nullptr)
			throw std::runtime_error("duplicate NO_PASSWORD packet");

		response.no_password = string_payload.data();
		return;
#else
		break;
#endif

	case TranslationCommand::REALM_SESSION:
#if TRANSLATION_ENABLE_SESSION
		response.realm_session = payload;
		return;
#else
		break;
#endif

	case TranslationCommand::WRITE_FILE:
		previous_command = command;
		HandleWriteFile(string_payload);
		return;

	case TranslationCommand::PATH_EXISTS:
#if TRANSLATION_ENABLE_RADDRESS
		if (!payload.empty())
			throw std::runtime_error("malformed PATH_EXISTS packet");

		if (response.path_exists)
			throw std::runtime_error("duplicate PATH_EXISTS packet");

		response.path_exists = true;
		return;
#else
		break;
#endif

	case TranslationCommand::AUTHORIZED_KEYS:
#if TRANSLATION_ENABLE_LOGIN
		if (!IsValidNonEmptyString(string_payload))
			throw std::runtime_error("malformed AUTHORIZED_KEYS packet");

		if (response.authorized_keys != nullptr)
			throw std::runtime_error("duplicate AUTHORIZED_KEYS packet");

		response.authorized_keys = string_payload.data();
		return;
#else
		break;
#endif

	case TranslationCommand::AUTO_BROTLI:
#if TRANSLATION_ENABLE_RADDRESS
		if (!payload.empty())
			throw std::runtime_error("malformed AUTO_BROTLI packet");

		if (response.auto_brotli)
			throw std::runtime_error("misplaced AUTO_BROTLI packet");

		response.auto_brotli = true;
		return;
#else
		break;
#endif

	case TranslationCommand::DISPOSABLE:
#if TRANSLATION_ENABLE_RADDRESS
		if (!payload.empty())
			throw std::runtime_error("malformed DISPOSABLE packet");

		if (cgi_address != nullptr &&
		    resource_address->type == ResourceAddress::Type::WAS &&
		    cgi_address->concurrency == 0)
			cgi_address->disposable = true;
		else
			throw std::runtime_error("misplaced DISPOSABLE packet");
		return;
#else
		break;
#endif

	case TranslationCommand::DISCARD_QUERY_STRING:
#if TRANSLATION_ENABLE_HTTP
		if (!payload.empty())
			throw std::runtime_error("malformed DISCARD_QUERY_STRING packet");

		if (response.discard_query_string)
			throw std::runtime_error("duplicate DISCARD_QUERY_STRING packet");

		response.discard_query_string = true;
		return;
#else
		break;
#endif

	case TranslationCommand::MOUNT_NAMED_TMPFS:
		HandleMountNamedTmpfs(string_payload);
		return;

	case TranslationCommand::BENEATH:
#if TRANSLATION_ENABLE_RADDRESS
		if (!IsValidAbsolutePath(string_payload))
			throw std::runtime_error("malformed BENEATH packet");

		if (file_address == nullptr)
			throw std::runtime_error("misplaced BENEATH packet");

		if (file_address->beneath != nullptr)
			throw std::runtime_error("duplicate BENEATH packet");

		file_address->beneath = string_payload.data();
		return;
#else
		break;
#endif
	}

	throw FmtRuntimeError("unknown translation packet: {}", (unsigned)command);
}

inline TranslateParser::Result
TranslateParser::HandlePacket(TranslationCommand command,
			      std::span<const std::byte> payload)
{
	if (command == TranslationCommand::BEGIN) {
		if (begun)
			throw std::runtime_error("double BEGIN from translation server");
	} else {
		if (!begun)
			throw std::runtime_error("no BEGIN from translation server");
	}

	switch (command) {
	case TranslationCommand::END:
#if TRANSLATION_ENABLE_RADDRESS
		FinishAddressList();
#endif

		FinishTranslateResponse(alloc,
#if TRANSLATION_ENABLE_RADDRESS
					base_suffix,
					std::move(layout_items_builder),
#endif
					response,
					{probe_suffixes_builder.data(),
					 probe_suffixes_builder.size()});

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
#if TRANSLATION_ENABLE_EXECUTE
		SetChildOptions(response.child_options);
#else
		child_options = nullptr;
		ns_options = nullptr;
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
		response.views.clear();
		response.views.push_front(*alloc.New<WidgetView>(nullptr));
		view = nullptr;
		widget_view_tail = response.views.begin();
#endif

#if TRANSLATION_ENABLE_TRANSFORMATION
		transformation = nullptr;
		transformation_tail = response.views.front().transformations.before_begin();
		filter = nullptr;
#endif

		if (payload.size() >= sizeof(uint8_t))
			response.protocol_version = *(const uint8_t *)payload.data();

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
