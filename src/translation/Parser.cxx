// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Parser.hxx"
#include "Response.hxx"
#if TRANSLATION_ENABLE_EXECUTE
#include "ExecuteOptions.hxx"
#endif
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
#include "http/local/Address.hxx"
#include "http/Address.hxx"
#include "cgi/Address.hxx"
#include "uri/Base.hxx"
#endif
#if TRANSLATION_ENABLE_SPAWN
#include "spawn/ChildOptions.hxx"
#include "spawn/Mount.hxx"
#include "spawn/ResourceLimits.hxx"
#endif
#if TRANSLATION_ENABLE_HTTP
#include "net/AllocatedSocketAddress.hxx"
#include "net/AddressInfo.hxx"
#include "net/Parser.hxx"
#include "http/Status.hxx"
#endif
#include "lib/fmt/RuntimeError.hxx"
#include "system/Arch.hxx"
#include "util/CharUtil.hxx"
#include "util/SpanCast.hxx"
#include "util/StringCompare.hxx"
#include "util/StringSplit.hxx"
#include "util/StringVerify.hxx"
#include "util/StringListVerify.hxx"
#include "util/Unaligned.hxx"

#if TRANSLATION_ENABLE_HTTP
#include "http/HeaderName.hxx"
#endif

#include <algorithm>
#include <utility> // for std::unreachable()

#include <assert.h>
#include <string.h>

using std::string_view_literals::operator""sv;

#if TRANSLATION_ENABLE_SPAWN

inline bool
TranslateParser::HasArgs() const noexcept
{
#if TRANSLATION_ENABLE_RADDRESS
	if (cgi_address != nullptr || lhttp_address != nullptr)
		return true;
#endif

#if TRANSLATION_ENABLE_EXECUTE
	if (const auto *options = GetExecuteOptions();
	    options != nullptr && options->execute != nullptr)
		return true;
#endif

	return false;
}

inline void
TranslateParser::SetChildOptions(ChildOptions &_child_options) noexcept
{
	child_options = &_child_options;
	mount_list = child_options->ns.mount.mounts.before_begin();
	env_builder = child_options->env;
}

inline NamespaceOptions *
TranslateParser::GetNamespaceOptions() noexcept
{
	return child_options != nullptr
		? &child_options->ns
		: nullptr;
}

#if TRANSLATION_ENABLE_EXECUTE

inline const ExecuteOptions *
TranslateParser::GetExecuteOptions() const noexcept
{
	return execute_options;
}

inline ExecuteOptions &
TranslateParser::MakeExecuteOptions(const char *error_message)
{
	if (execute_options != nullptr)
		return *execute_options;

	if (response.execute_options == nullptr) {
		execute_options = response.execute_options = alloc.New<ExecuteOptions>();
		SetChildOptions(execute_options->child_options);
		return *execute_options;
	}

	throw std::runtime_error{error_message};
}

#endif

ChildOptions &
TranslateParser::MakeChildOptions(const char *error_message)
{
#if TRANSLATION_ENABLE_EXECUTE
	if (child_options == nullptr && response.execute_options == nullptr)
		return MakeExecuteOptions(error_message).child_options;
#endif

	if (child_options == nullptr)
		throw std::runtime_error{error_message};

	return *child_options;
}

inline ExpandableStringList::Builder &
TranslateParser::MakeEnvBuilder(const char *error_message)
{
	MakeChildOptions(error_message);
	return env_builder;
}

inline NamespaceOptions &
TranslateParser::MakeNamespaceOptions(const char *error_message)
{
	return MakeChildOptions(error_message).ns;
}

inline MountNamespaceOptions &
TranslateParser::MakeMountNamespaceOptions(const char *error_message)
{
	return MakeNamespaceOptions(error_message).mount;
}

void
TranslateParser::AddMount(const char *error_message, Mount *mount)
{
	auto &options = MakeMountNamespaceOptions(error_message);
	mount_list = options.mounts.insert_after(mount_list, *mount);
}

#endif // TRANSLATION_ENABLE_SPAWN

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

#endif

#if TRANSLATION_ENABLE_SPAWN

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

#endif // TRANSLATION_ENABLE_SPAWN

#if TRANSLATION_ENABLE_WIDGET

void
TranslateParser::FinishView()
{
	assert(!response.views.empty());

	FinishAddressList();

	WidgetView *v = view;
	if (view == nullptr) {
		v = &response.views.front();
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
#if TRANSLATION_ENABLE_EXECUTE
	execute_options = nullptr;
#endif
	child_options = nullptr;
	file_address = nullptr;
	http_address = nullptr;
	cgi_address = nullptr;
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

		if (response.base != nullptr) {
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

#if TRANSLATION_ENABLE_SPAWN

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

#endif // TRANSLATION_ENABLE_SPAWN

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

#if TRANSLATION_ENABLE_SPAWN

inline void
TranslateParser::HandlePivotRoot(std::string_view payload)
{
	if (!IsValidAbsolutePath(payload))
		throw std::runtime_error("malformed PIVOT_ROOT packet");

	auto &options = MakeMountNamespaceOptions("misplaced PIVOT_ROOT packet");

	if (options.pivot_root != nullptr ||
	    options.mount_root_tmpfs)
		throw std::runtime_error("duplicate PIVOT_ROOT packet");

	options.pivot_root = payload.data();
}

inline void
TranslateParser::HandleMountRootTmpfs(std::string_view payload)
{
	if (!payload.empty())
		throw std::runtime_error("malformed MOUNT_ROOT_TMPFS packet");

	auto &options = MakeMountNamespaceOptions("misplaced MOUNT_ROOT_TMPFS packet");

	if (options.pivot_root != nullptr ||
	    options.mount_root_tmpfs)
		throw std::runtime_error("duplicate MOUNT_ROOT_TMPFS packet");

	options.mount_root_tmpfs = true;
}

inline void
TranslateParser::HandleHome(std::string_view payload)
{
	if (!IsValidAbsolutePath(payload))
		throw std::runtime_error("malformed HOME packet");

	auto &options = MakeMountNamespaceOptions("misplaced HOME packet");

	if (options.home != nullptr)
		throw std::runtime_error("duplicate HOME packet");

	options.home = payload.data();
}

#if TRANSLATION_ENABLE_EXPAND

inline void
TranslateParser::HandleExpandHome(std::string_view payload)
{
	if (!IsValidAbsolutePath(payload))
		throw std::runtime_error("malformed EXPAND_HOME packet");

	auto &options = MakeMountNamespaceOptions("misplaced EXPAND_HOME packet");
	if (options.expand_home)
		throw std::runtime_error{"duplicate EXPAND_HOME packet"};

	options.expand_home = true;
	options.home = payload.data();
}

#endif

inline void
TranslateParser::HandleMountProc(std::string_view payload)
{
	if (!payload.empty())
		throw std::runtime_error("malformed MOUNT_PROC packet");

	auto &options = MakeMountNamespaceOptions("misplaced MOUNT_PROC packet");
	if (options.mount_proc)
		throw std::runtime_error("duplicate MOUNT_PROC packet");

	options.mount_proc = true;
}

inline void
TranslateParser::HandleMountTmpTmpfs(std::string_view payload)
{
	if (!IsValidString(payload))
		throw std::runtime_error("malformed MOUNT_TMP_TMPFS packet");

	auto &options = MakeMountNamespaceOptions("misplaced MOUNT_TMP_TMPFS packet");

	if (options.mount_tmp_tmpfs != nullptr)
		throw std::runtime_error("duplicate MOUNT_TMP_TMPFS packet");

	options.mount_tmp_tmpfs = payload.data() != nullptr
		? payload.data()
		: "";
}

inline void
TranslateParser::HandleMountHome(std::string_view payload)
{
	if (!IsValidAbsolutePath(payload))
		throw std::runtime_error("malformed MOUNT_HOME packet");

	auto &options = MakeMountNamespaceOptions("misplaced RLIMITS packet");
	if (options.home == nullptr)
		throw std::runtime_error("misplaced MOUNT_HOME packet");

	if (options.HasMountOn(payload.data()))
		throw std::runtime_error{"duplicate MOUNT_HOME packet"};

	auto *m = alloc.New<Mount>(/* skip the slash to make it relative */
		options.home + 1,
		payload.data(),
		true, true);

#if TRANSLATION_ENABLE_EXPAND
	m->expand_source = options.expand_home;
#endif

	mount_list = options.mounts.insert_after(mount_list, *m);

	assert(options.HasMountOn(payload.data()));
}

inline void
TranslateParser::HandleMountTmpfs(std::string_view payload, bool writable)
{
	if (!IsValidAbsolutePath(payload) ||
	    /* not allowed for /tmp, use MOUNT_TMP_TMPFS
	       instead! */
	    payload == "/tmp"sv)
		throw std::runtime_error("malformed MOUNT_TMPFS packet");

	AddMount("misplaced MOUNT_TMPFS packet",
		 alloc.New<Mount>(Mount::Tmpfs{}, payload.data(), writable));
}

inline void
TranslateParser::HandleMountNamedTmpfs(std::string_view payload)
{
	const auto [source, target] = Split(payload, '\0');
	if (!IsValidName(source) ||
	    !IsValidAbsolutePath(target))
		throw std::runtime_error("malformed MOUNT_NAMED_TMPFS packet");

	AddMount("misplaced MOUNT_NAMED_TMPFS packet",
		 alloc.New<Mount>(Mount::NamedTmpfs{},
				  source.data(),
				  target.data(),
				  true));
}

inline void
TranslateParser::HandleBindMount(std::string_view payload,
				 bool expand, bool writable, bool exec,
				 bool file)
{
	auto [_source, target] = Split(payload, '\0');
	if (!IsValidAbsolutePath(target))
		throw std::runtime_error("malformed BIND_MOUNT packet");

	const char *source;
	if (SkipPrefix(_source, "container:"sv)) {
		/* path on host (host's mount namespace) */

		if (!IsValidAbsolutePath(_source))
			throw std::runtime_error{"malformed BIND_MOUNT packet"};

		/* keep the slash */
		source = _source.data();
	} else {
		/* no prefix or prefix "host:": path on host (host's
		   mount namespace) */
		SkipPrefix(_source, "host:"sv);

		if (!IsValidAbsolutePath(_source))
			throw std::runtime_error{"malformed BIND_MOUNT packet"};

		/* skip the slash to make it relative to the working
		   directory (which is the host mount) */
		source = _source.data() + 1;
	}

	auto *m = alloc.New<Mount>(
		source,
		target.data(),
		writable, exec);
#if TRANSLATION_ENABLE_EXPAND
	m->expand_source = expand;
#else
	(void)expand;
#endif

	if (file)
		m->type = Mount::Type::BIND_FILE;

	AddMount("misplaced BIND_MOUNT packet", m);
}

inline void
TranslateParser::HandleSymlink(std::string_view payload)
{
	const auto [target, linkpath] = Split(payload, '\0');
	if (!IsValidNonEmptyString(target) ||
	    !IsValidAbsolutePath(linkpath))
		throw std::runtime_error("malformed SYMLINK packet");

	auto *m = alloc.New<Mount>(target.data(), linkpath.data());
	m->type = Mount::Type::SYMLINK;
	AddMount("misplaced SYMLINK packet", m);
}

inline void
TranslateParser::HandleWriteFile(std::string_view payload)
{
	const auto [path, contents] = Split(payload, '\0');
	if (!IsValidAbsolutePath(path) || !IsValidString(contents))
		throw std::runtime_error("malformed WRITE_FILE packet");

	auto *m = alloc.New<Mount>(Mount::WriteFile{},
				   path.data(),
				   contents.data());

	AddMount("misplaced WRITE_FILE packet", m);
}

inline void
TranslateParser::HandleUtsNamespace(std::string_view payload)
{
	if (!IsValidNonEmptyString(payload))
		throw std::runtime_error{"malformed UTS_NAMESPACE packet"};

	auto &options = MakeNamespaceOptions("misplaced UTS_NAMESPACE packet");
	if (options.hostname != nullptr)
		throw std::runtime_error{"misplaced UTS_NAMESPACE packet"};

	options.hostname = payload.data();
}

inline void
TranslateParser::HandleRlimits(std::string_view payload)
{
	auto &options = MakeChildOptions("misplaced RLIMITS packet");

	if (options.rlimits == nullptr)
		options.rlimits = alloc.New<ResourceLimits>();

	if (!options.rlimits->Parse(payload))
		throw std::runtime_error("malformed RLIMITS packet");
}

#endif // TRANSLATION_ENABLE_SPAWN

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

#if TRANSLATION_ENABLE_SPAWN

inline void
TranslateParser::HandleStderrPath(std::string_view payload, bool jailed)
{
	if (!IsValidAbsolutePath(payload))
		throw std::runtime_error("malformed STDERR_PATH packet");

	auto &options = MakeChildOptions("misplaced STDERR_PATH packet");

	if (options.stderr_null)
		throw std::runtime_error("misplaced STDERR_PATH packet");

	if (options.stderr_path != nullptr)
		throw std::runtime_error("duplicate STDERR_PATH packet");

	options.stderr_path = payload.data();
	options.stderr_jailed = jailed;
}

#if TRANSLATION_ENABLE_EXPAND

inline void
TranslateParser::HandleExpandStderrPath(std::string_view payload)
{
	if (!IsValidNonEmptyString(payload))
		throw std::runtime_error("malformed EXPAND_STDERR_PATH packet");

	auto &options = MakeChildOptions("misplaced EXPAND_STDERR_PATH packet");

	if (options.expand_stderr_path != nullptr)
		throw std::runtime_error("duplicate EXPAND_STDERR_PATH packet");

	options.expand_stderr_path = payload.data();
}

#endif

inline void
TranslateParser::HandleUidGid(std::span<const std::byte> _payload)
{
	auto &uid_gid = MakeChildOptions("misplaced RLIMITS packet").uid_gid;

	if (!uid_gid.IsEmpty())
		throw std::runtime_error("duplicate UID_GID packet");

	constexpr size_t min_size = sizeof(int) * 2;
	const size_t max_size = min_size + sizeof(int) * uid_gid.supplementary_groups.max_size();

	if (_payload.size() < min_size || _payload.size() > max_size ||
	    _payload.size() % sizeof(int) != 0)
		throw std::runtime_error("malformed UID_GID packet");

	const auto payload = FromBytesFloor<const int>(_payload);
	uid_gid.effective_uid = payload[0];
	uid_gid.effective_gid = payload[1];

	size_t n_groups = payload.size() - 2;
	std::copy(std::next(payload.begin(), 2), payload.end(),
		  uid_gid.supplementary_groups.begin());
	if (n_groups < uid_gid.supplementary_groups.max_size())
		uid_gid.supplementary_groups[n_groups] = UidGid::UNSET_GID;
}

inline void
TranslateParser::HandleMappedUidGid(std::span<const std::byte> payload)
{
	auto &options = MakeChildOptions("misplaced MAPPED_UID_GID packet");

	if (options.uid_gid.effective_uid == UidGid::UNSET_UID ||
	    !options.ns.enable_user)
		throw std::runtime_error{"misplaced MAPPED_UID_GID packet"};

	const auto *value = (const uint32_t *)(const void *)payload.data();
	if (payload.size() != sizeof(*value) || *value <= 0)
		throw std::runtime_error{"malformed MAPPED_UID_GID packet"};

	if (options.ns.mapped_effective_uid != 0)
		throw std::runtime_error{"duplicate MAPPED_UID_GID packet"};

	options.ns.mapped_effective_uid = *value;
}

inline void
TranslateParser::HandleMappedRealUidGid(std::span<const std::byte> payload)
{
	auto &options = MakeChildOptions("misplaced MAPPED_REAL_UID_GID packet");

	if (options.uid_gid.real_uid == UidGid::UNSET_UID ||
	    !options.ns.enable_user)
		throw std::runtime_error{"misplaced MAPPED_REAL_UID_GID packet"};

	const auto *value = (const uint32_t *)(const void *)payload.data();
	if (payload.size() != sizeof(*value) || *value <= 0)
		throw std::runtime_error{"malformed MAPPED_REAL_UID_GID packet"};

	if (options.ns.mapped_real_uid != 0)
		throw std::runtime_error{"duplicate MAPPED_REAL_UID_GID packet"};

	options.ns.mapped_real_uid = *value;
}

inline void
TranslateParser::HandleRealUidGid(std::span<const std::byte> payload)
{
	auto &options = MakeChildOptions("misplaced REAL_UID packet");

	if (options.uid_gid.IsEmpty())
		throw std::runtime_error{"misplaced REAL_UID_GID packet"};

	if (options.uid_gid.HasReal())
		throw std::runtime_error{"duplicate REAL_UID_GID packet"};

	if (payload.size() < sizeof(options.uid_gid.real_uid))
		throw std::runtime_error{"malformed REAL_UID_GID packet"};

	LoadUnaligned(options.uid_gid.real_uid, payload.data());
	payload = payload.subspan(sizeof(options.uid_gid.real_uid));

	if (payload.size() >= sizeof(options.uid_gid.real_gid)) {
		LoadUnaligned(options.uid_gid.real_gid, payload.data());
		payload = payload.subspan(sizeof(options.uid_gid.real_gid));
	}

	if (!payload.empty())
		throw std::runtime_error{"malformed REAL_UID_GID packet"};

	if (!options.uid_gid.HasReal())
		throw std::runtime_error{"malformed REAL_UID_GID packet"};
}

inline void
TranslateParser::HandleUmask(std::span<const std::byte> payload)
{
	typedef uint16_t value_type;

	auto &options = MakeChildOptions("misplaced UMASK packet");

	if (options.umask >= 0)
		throw std::runtime_error("duplicate UMASK packet");

	if (payload.size() != sizeof(value_type))
		throw std::runtime_error("malformed UMASK packet");

	auto umask = *(const uint16_t *)(const void *)payload.data();
	if (umask & ~0777)
		throw std::runtime_error("malformed UMASK packet");

	options.umask = umask;
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
	auto &options = MakeChildOptions("misplaced CGROUP_SET packet");

	auto set = ParseCgroupSet(payload);
	if (set.first.data() == nullptr)
		throw std::runtime_error("malformed CGROUP_SET packet");

	options.cgroup.Set(alloc, set.first, set.second);
}

inline void
TranslateParser::HandleCgroupXattr(std::string_view payload)
{
	auto &options = MakeChildOptions("misplaced CGROUP_XATTR packet");

	auto xattr = ParseCgroupSet(payload);
	if (xattr.first.data() == nullptr)
		throw std::runtime_error("malformed CGROUP_XATTR packet");

	options.cgroup.SetXattr(alloc, xattr.first, xattr.second);
}

inline void
TranslateParser::HandleMountListenStream(std::span<const std::byte> payload)
{
	auto &options = MakeMountNamespaceOptions("misplaced MOUNT_LISTEN_STREAM packet");

	if (options.mount_listen_stream.data() != nullptr)
		throw std::runtime_error("duplicate MOUNT_LISTEN_STREAM packet");

	const auto [path, rest] = Split(ToStringView(payload), '\0');
	if (!IsValidAbsolutePath(path))
		throw std::runtime_error("malformed MOUNT_LISTEN_STREAM packet");

	options.mount_listen_stream = payload;
}

#endif // TRANSLATION_ENABLE_SPAWN

#if TRANSLATION_ENABLE_HTTP

inline void
TranslateParser::HandleAllowRemoteNetwork(std::span<const std::byte> payload)
{
	if (payload.size() < 2)
		throw std::runtime_error{"malformed ALLOW_REMOTE_NETWORK packet"};

	const uint_least8_t prefix_length = static_cast<uint8_t>(payload.front());
	const SocketAddress address{
		reinterpret_cast<const struct sockaddr *>(payload.data() + 1),
		static_cast<SocketAddress::size_type>(payload.size() - 1),
	};

	response.allow_remote_networks.Add(alloc, address, prefix_length);
}

inline void
TranslateParser::HandleTokenBucketParams(TranslateTokenBucketParams &params,
					 const char *packet_name,
					 std::span<const std::byte> payload)
{
	if (params.IsDefined())
		throw FmtRuntimeError("duplicate {} packet", packet_name);

	if (payload.size() != sizeof(params))
		throw FmtRuntimeError("malformed {} packet", packet_name);

	memcpy(&params, payload.data(), sizeof(params));
	if (!params.IsValid())
		throw FmtRuntimeError("malformed {} packet", packet_name);
}

#endif // TRANSLATION_ENABLE_HTTP

static bool
CheckProbeSuffix(std::string_view payload) noexcept
{
	return payload.find('/') == payload.npos &&
		IsValidString(payload);
}

inline void
TranslateParser::HandleRegularPacket(TranslationCommand command,
				     const std::span<const std::byte> payload)
{
	const std::string_view string_payload = ToStringView(payload);

	switch (command) {
	case TranslationCommand::BEGIN:
	case TranslationCommand::END:
		std::unreachable();

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
	case TranslationCommand::ALT_HOST:
	case TranslationCommand::CHAIN_HEADER:
	case TranslationCommand::AUTH_TOKEN:
	case TranslationCommand::PLAN:
		throw std::runtime_error("misplaced translate request packet");

	case TranslationCommand::UID_GID:
#if TRANSLATION_ENABLE_SPAWN
		HandleUidGid(payload);
		return;
#else
		break;
#endif

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
#if TRANSLATION_ENABLE_EXECUTE
		execute_options = nullptr;
#endif
		child_options = nullptr;
		file_address = nullptr;
		cgi_address = nullptr;
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
		throw std::runtime_error{"NFS support has been removed"};
#else
		break;
#endif

	case TranslationCommand::NFS_EXPORT:
#if TRANSLATION_ENABLE_RADDRESS
		throw std::runtime_error{"NFS support has been removed"};
#else
		break;
#endif

	case TranslationCommand::JAILCGI:
		/* obsolete */
		break;

	case TranslationCommand::HOME:
#if TRANSLATION_ENABLE_SPAWN
		HandleHome(string_payload);
		return;
#else
		break;
#endif

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
		    payload.size() > 16 * sizeof(response.vary.front()) ||
		    payload.size() % sizeof(response.vary.front()) != 0)
			throw std::runtime_error("malformed VARY packet");

		response.vary = FromBytesFloor<const TranslationCommand>(payload);
#endif
		return;

	case TranslationCommand::INVALIDATE:
#if TRANSLATION_ENABLE_CACHE
		if (payload.empty() ||
		    payload.size() > 16 * sizeof(response.invalidate.front()) ||
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
		throw std::runtime_error{"deprecated DELEGATE packet"};

	case TranslationCommand::APPEND:
#if TRANSLATION_ENABLE_SPAWN
		if (!IsValidNonEmptyString(string_payload))
			throw std::runtime_error("malformed APPEND packet");

		if (!HasArgs())
			throw std::runtime_error("misplaced APPEND packet");

		args_builder.Add(alloc, string_payload.data(), false);
		return;
#else
		break;
#endif

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

#if TRANSLATION_ENABLE_SPAWN
		MakeChildOptions("misplaced PAIR packet");
		translate_client_pair(alloc, env_builder, "PAIR",
				      string_payload);
		return;
#else
		break;
#endif // TRANSLATION_ENABLE_SPAWN

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
#if TRANSLATION_ENABLE_SPAWN
		if (!payload.empty())
			throw std::runtime_error("malformed USER_NAMESPACE packet");

		MakeNamespaceOptions("misplaced USER_NAMESPACE packet").enable_user = true;
		return;
#else
		break;
#endif

	case TranslationCommand::PID_NAMESPACE:
#if TRANSLATION_ENABLE_SPAWN
		if (!payload.empty())
			throw std::runtime_error("malformed PID_NAMESPACE packet");

		if (auto &options = MakeNamespaceOptions("misplaced PID_NAMESPACE packet");
		    options.pid_namespace != nullptr)
			throw std::runtime_error("Can't combine PID_NAMESPACE with PID_NAMESPACE_NAME");
		else
			options.enable_pid = true;

		return;
#else
		break;
#endif

	case TranslationCommand::NETWORK_NAMESPACE:
#if TRANSLATION_ENABLE_SPAWN
		if (!payload.empty())
			throw std::runtime_error("malformed NETWORK_NAMESPACE packet");

		if (auto &options = MakeNamespaceOptions("misplaced NETWORK_NAMESPACE packet");
		    options.enable_network)
			throw std::runtime_error("duplicate NETWORK_NAMESPACE packet");
		else if (options.network_namespace != nullptr)
			throw std::runtime_error("Can't combine NETWORK_NAMESPACE with NETWORK_NAMESPACE_NAME");
		else
			options.enable_network = true;
		return;
#else
		break;
#endif

	case TranslationCommand::PIVOT_ROOT:
#if TRANSLATION_ENABLE_SPAWN
		HandlePivotRoot(string_payload);
		return;
#else
		break;
#endif

	case TranslationCommand::MOUNT_PROC:
#if TRANSLATION_ENABLE_SPAWN
		HandleMountProc(string_payload);
		return;
#else
		break;
#endif

	case TranslationCommand::MOUNT_HOME:
#if TRANSLATION_ENABLE_SPAWN
		HandleMountHome(string_payload);
		return;
#else
		break;
#endif

	case TranslationCommand::BIND_MOUNT:
#if TRANSLATION_ENABLE_SPAWN
		previous_command = command;
		HandleBindMount(string_payload, false, false);
		return;
#else
		break;
#endif

	case TranslationCommand::MOUNT_TMP_TMPFS:
#if TRANSLATION_ENABLE_SPAWN
		HandleMountTmpTmpfs(string_payload);
		return;
#else
		break;
#endif

	case TranslationCommand::UTS_NAMESPACE:
#if TRANSLATION_ENABLE_SPAWN
		HandleUtsNamespace(string_payload);
		return;
#else
		break;
#endif

	case TranslationCommand::RLIMITS:
#if TRANSLATION_ENABLE_SPAWN
		HandleRlimits(string_payload);
		return;
#else
		break;
#endif

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
#if TRANSLATION_ENABLE_SPAWN
		HandleStderrPath(string_payload, false);
		return;
#else
		break;
#endif

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
#if TRANSLATION_ENABLE_SPAWN
		translate_client_pair(alloc, MakeEnvBuilder("misplaced SETENV packet"),
				      "SETENV",
				      string_payload);
		return;
#else
		break;
#endif

	case TranslationCommand::EXPAND_SETENV:
#if TRANSLATION_ENABLE_EXPAND
		if (response.regex == nullptr)
			throw std::runtime_error("misplaced EXPAND_SETENV packet");

		translate_client_expand_pair(MakeEnvBuilder("misplaced EXPAND_SETENV packet"),
					     "EXPAND_SETENV",
					     string_payload);
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
#if TRANSLATION_ENABLE_SPAWN
		if (!payload.empty())
			throw std::runtime_error("malformed IPC_NAMESPACE packet");

		MakeNamespaceOptions("misplaced IPC_NAMESPACE packet").enable_ipc = true;
		return;
#else
		break;
#endif

	case TranslationCommand::AUTO_DEFLATE:
		/* deprecated */
		return;

	case TranslationCommand::EXPAND_HOME:
#if TRANSLATION_ENABLE_EXPAND
		return HandleExpandHome(string_payload);
#else
		break;
#endif

	case TranslationCommand::EXPAND_STDERR_PATH:
#if TRANSLATION_ENABLE_EXPAND
		HandleExpandStderrPath(string_payload);
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

	case TranslationCommand::SERVICE:
#if TRANSLATION_ENABLE_EXECUTE_SERVICE
		if (!IsValidNonEmptyString(string_payload))
			throw std::runtime_error{"malformed SERVICE packet"};

		if (response.execute_options == nullptr)
			throw std::runtime_error{"misplaced SERVICE packet"};

		execute_options = &response.service_execute_options.Add(alloc, string_payload.data());
		SetChildOptions(execute_options->child_options);
		return;
#else
		break;
#endif

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
#if TRANSLATION_ENABLE_SPAWN
		previous_command = command;
		HandleBindMount(string_payload, false, true);
		return;
#else
		break;
#endif

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
#if TRANSLATION_ENABLE_SPAWN
		HandleMountTmpfs(string_payload, true);
		return;
#else
		break;
#endif

	case TranslationCommand::MOUNT_EMPTY:
#if TRANSLATION_ENABLE_SPAWN
		HandleMountTmpfs(string_payload, false);
		return;
#else
		break;
#endif

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
#if TRANSLATION_ENABLE_SPAWN && defined(HAVE_LIBSECCOMP)
		if (!payload.empty())
			throw std::runtime_error("malformed FORBID_USER_NS packet");

		if (auto &options = MakeChildOptions("misplaced FORBID_USER_NS packet");
		    options.forbid_user_ns)
			throw std::runtime_error("duplicate FORBID_USER_NS packet");
		else
			options.forbid_user_ns = true;
		return;
#else
		break;
#endif

	case TranslationCommand::NO_NEW_PRIVS:
#if TRANSLATION_ENABLE_SPAWN
		if (!payload.empty())
			throw std::runtime_error("malformed NO_NEW_PRIVS packet");

		if (auto &options = MakeChildOptions("misplaced NO_NEW_PRIVS packet");
		    options.no_new_privs)
			throw std::runtime_error("duplicate NO_NEW_PRIVS packet");
		else
			options.no_new_privs = true;
		return;
#else
		break;
#endif

	case TranslationCommand::CGROUP:
#if TRANSLATION_ENABLE_SPAWN
		if (!valid_view_name(string_payload.data()))
			throw std::runtime_error("malformed CGROUP packet");

		if (auto &options = MakeChildOptions("misplaced CGROUP packet");
		    options.cgroup.name != nullptr)
			throw std::runtime_error("duplicate CGROUP packet");
		else
			options.cgroup.name = string_payload.data();
		return;
#else
		break;
#endif

	case TranslationCommand::CGROUP_SET:
#if TRANSLATION_ENABLE_SPAWN
		HandleCgroupSet(string_payload);
		return;
#else
		break;
#endif

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
#if TRANSLATION_ENABLE_SPAWN
		previous_command = command;
		HandleBindMount(string_payload, false, false, true);
		return;
#else
		break;
#endif

	case TranslationCommand::EXPAND_BIND_MOUNT_EXEC:
#if TRANSLATION_ENABLE_EXPAND
		previous_command = command;
		HandleBindMount(string_payload, true, false, true);
		return;
#else
		break;
#endif

	case TranslationCommand::STDERR_NULL:
#if TRANSLATION_ENABLE_SPAWN
		if (!payload.empty())
			throw std::runtime_error("malformed STDERR_NULL packet");

		if (auto &options = MakeChildOptions("misplaced STDERR_NULL packet");
		    options.stderr_null || options.stderr_path != nullptr)
			throw std::runtime_error("duplicate STDERR_NULL packet");
		else
			options.stderr_null = true;
		return;
#else
		break;
#endif

	case TranslationCommand::EXECUTE:
#if TRANSLATION_ENABLE_EXECUTE
		if (!IsValidAbsolutePath(string_payload))
			throw std::runtime_error("malformed EXECUTE packet");

		if (auto &options = MakeExecuteOptions("misplaced EXECUTE pacxket");
		    options.execute != nullptr)
			throw std::runtime_error("duplicate EXECUTE packet");
		else {
			options.execute = string_payload.data();
			args_builder = options.args;
		}

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

		if (auto &options = MakeExecuteOptions("misplaced SHELL packet");
		    options.shell != nullptr)
			throw std::runtime_error("duplicate SHELL packet");
		else
			options.shell = string_payload.data();
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
#if TRANSLATION_ENABLE_SPAWN
		HandleStderrPath(string_payload, true);
		return;
#else
		break;
#endif

	case TranslationCommand::UMASK:
#if TRANSLATION_ENABLE_SPAWN
		HandleUmask(payload);
		return;
#else
		break;
#endif

	case TranslationCommand::CGROUP_NAMESPACE:
#if TRANSLATION_ENABLE_SPAWN
		if (!payload.empty())
			throw std::runtime_error("malformed CGROUP_NAMESPACE packet");

		if (auto &options = MakeNamespaceOptions("misplaced CGROUP_NAMESPACE packet");
		    options.enable_cgroup)
			throw std::runtime_error("duplicate CGROUP_NAMESPACE packet");
		else
			options.enable_cgroup = true;

		return;
#else
		break;
#endif

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
#if TRANSLATION_ENABLE_SPAWN && defined(HAVE_LIBSECCOMP)
		if (!payload.empty())
			throw std::runtime_error("malformed FORBID_MULTICAST packet");

		if (auto &options = MakeChildOptions("misplaced FORBID_MULTICAST packet");
		    options.forbid_multicast)
			throw std::runtime_error("duplicate FORBID_MULTICAST packet");
		else
			options.forbid_multicast = true;
		return;
#else
		break;
#endif

	case TranslationCommand::FORBID_BIND:
#if TRANSLATION_ENABLE_SPAWN && defined(HAVE_LIBSECCOMP)
		if (!payload.empty())
			throw std::runtime_error("malformed FORBID_BIND packet");

		if (auto &options = MakeChildOptions("misplaced FORBID_BIND packet");
		    options.forbid_bind)
			throw std::runtime_error("duplicate FORBID_BIND packet");
		else
			options.forbid_bind = true;
		return;
#else
		break;
#endif

	case TranslationCommand::NETWORK_NAMESPACE_NAME:
#if TRANSLATION_ENABLE_SPAWN
		if (!IsValidName(string_payload))
			throw std::runtime_error("malformed NETWORK_NAMESPACE_NAME packet");

		if (auto &options = MakeNamespaceOptions("misplaced NETWORK_NAMESPACE_NAME packet");
		    options.network_namespace != nullptr)
			throw std::runtime_error("duplicate NETWORK_NAMESPACE_NAME packet");
		else if (options.enable_network)
			throw std::runtime_error("Can't combine NETWORK_NAMESPACE_NAME with NETWORK_NAMESPACE");
		else
			options.network_namespace = string_payload.data();
		return;
#else
		break;
#endif

	case TranslationCommand::MOUNT_ROOT_TMPFS:
#if TRANSLATION_ENABLE_SPAWN
		HandleMountRootTmpfs(string_payload);
		return;
#else
		break;
#endif

	case TranslationCommand::CHILD_TAG:
#if TRANSLATION_ENABLE_SPAWN
		if (!IsValidNonEmptyString(string_payload))
			throw std::runtime_error("malformed CHILD_TAG packet");

		if (auto &options = MakeChildOptions("misplaced CHILD_TAG packet");
		    options.tag.data() == nullptr)
			options.tag = string_payload;
		else
			options.tag =
				alloc.ConcatView(options.tag, '\0',
						 string_payload);

		return;
#else
		break;
#endif

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
#if TRANSLATION_ENABLE_SPAWN
		if (!IsValidName(string_payload))
			throw std::runtime_error("malformed PID_NAMESPACE_NAME packet");

		if (auto &options = MakeNamespaceOptions("misplaced PID_NAMESPACE_NAME packet");
		    options.pid_namespace != nullptr)
			throw std::runtime_error("duplicate PID_NAMESPACE_NAME packet");
		else if (options.enable_pid)
			throw std::runtime_error("Can't combine PID_NAMESPACE_NAME with PID_NAMESPACE");
		else
			options.pid_namespace = string_payload.data();
		return;
#else
		break;
#endif

	case TranslationCommand::SUBST_YAML_FILE:
		break;

	case TranslationCommand::SUBST_ALT_SYNTAX:
		break;

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
#if TRANSLATION_ENABLE_SPAWN
		if (!payload.empty())
			throw std::runtime_error("malformed STDERR_POND packet");

		if (auto &options = MakeChildOptions("misplaced STDERR_POND packet");
		    options.stderr_pond)
			throw std::runtime_error("duplicate STDERR_POND packet");
		else
			options.stderr_pond = true;
		return;
#else
		break;
#endif

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
		case TranslationCommand::BIND_MOUNT_RW_EXEC:
#if TRANSLATION_ENABLE_SPAWN
			if (auto *options = GetNamespaceOptions();
			    options != nullptr &&
			    mount_list != options->mount.mounts.before_begin()) {
				mount_list->optional = true;
				return;
			}
#endif

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
#if TRANSLATION_ENABLE_SPAWN
		if (!payload.empty())
			throw std::runtime_error("malformed MOUNT_DEV packet");

		if (auto &options = MakeMountNamespaceOptions("misplaced MOUNT_DEV packet");
		    (!options.mount_root_tmpfs &&
		     options.pivot_root == nullptr))
			throw std::runtime_error("misplaced MOUNT_DEV packet");
		else if (options.mount_dev)
			throw std::runtime_error("duplicate MOUNT_DEV packet");
		else
			options.mount_dev = true;
		return;
#else
		break;
#endif

	case TranslationCommand::BIND_MOUNT_FILE:
#if TRANSLATION_ENABLE_SPAWN
		previous_command = command;
		HandleBindMount(string_payload, false, false, false, true);
		return;
#else
		break;
#endif

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
#if TRANSLATION_ENABLE_SPAWN
		HandleCgroupXattr(string_payload);
		return;
#else
		break;
#endif

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
#if TRANSLATION_ENABLE_SPAWN
		if (!IsValidAbsolutePath(string_payload))
			throw std::runtime_error("malformed CHDIR packet");

		if (auto &options = MakeChildOptions("misplaced CHDIR packet");
		    options.chdir != nullptr)
			throw std::runtime_error("duplicate CHDIR packet");
		else
			options.chdir = string_payload.data();
		return;
#else
		break;
#endif

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
#if TRANSLATION_ENABLE_SPAWN
		previous_command = command;
		HandleWriteFile(string_payload);
		return;
#else
		break;
#endif

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
#if TRANSLATION_ENABLE_SPAWN
		HandleMountNamedTmpfs(string_payload);
		return;
#else
		break;
#endif

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

	case TranslationCommand::MAPPED_UID_GID:
#if TRANSLATION_ENABLE_SPAWN
		HandleMappedUidGid(payload);
		return;
#else
		break;
#endif

	case TranslationCommand::MAPPED_REAL_UID_GID:
#if TRANSLATION_ENABLE_SPAWN
		HandleMappedRealUidGid(payload);
		return;
#else
		break;
#endif

	case TranslationCommand::NO_HOME_AUTHORIZED_KEYS:
#if TRANSLATION_ENABLE_LOGIN
		if (!payload.empty())
			throw std::runtime_error("malformed NO_HOME_AUTHORIZED_KEYS packet");

		if (response.no_home_authorized_keys)
			throw std::runtime_error("misplaced NO_HOME_AUTHORIZED_KEYS packet");

		response.no_home_authorized_keys = true;
		return;
#else
		break;
#endif

	case TranslationCommand::TIMEOUT:
		if (payload.size() != 4)
			throw std::runtime_error("malformed TIMEOUT packet");

		response.timeout = std::chrono::seconds(*(const uint32_t *)(const void *)payload.data());
		return;

	case TranslationCommand::MOUNT_LISTEN_STREAM:
#if TRANSLATION_ENABLE_SPAWN
		HandleMountListenStream(payload);
		return;
#else
		break;
#endif

	case TranslationCommand::ANALYTICS_ID:
		if (!IsValidString(string_payload))
			throw std::runtime_error("malformed ANALYTICS_ID packet");
		response.analytics_id = string_payload.data();
		return;

	case TranslationCommand::GENERATOR:
		if (!IsValidName(string_payload))
			throw std::runtime_error("malformed GENERATOR packet");
		response.generator = string_payload.data();
		return;

	case TranslationCommand::IGNORE_NO_CACHE:
#if TRANSLATION_ENABLE_CACHE
#if TRANSLATION_ENABLE_RADDRESS
		if (resource_address == nullptr)
			throw std::runtime_error("misplaced IGNORE_NO_CACHE packet");
#endif

		if (response.ignore_no_cache)
			throw std::runtime_error("duplicate IGNORE_NO_CACHE packet");

		response.ignore_no_cache = true;
		return;
#else // !TRANSLATION_ENABLE_CACHE
		break;
#endif

	case TranslationCommand::AUTO_COMPRESS_ONLY_TEXT:
#if TRANSLATION_ENABLE_RADDRESS
		if (!payload.empty())
			throw std::runtime_error{"malformed AUTO_COMPRESS_ONLY_TEXT packet"};

		if (response.auto_compress_only_text)
			throw std::runtime_error{"duplicate AUTO_COMPRESS_ONLY_TEXT packet"};

		response.auto_compress_only_text = true;
		return;
#else
		break;
#endif

	case TranslationCommand::REGEX_RAW:
#if TRANSLATION_ENABLE_EXPAND
		if (!payload.empty())
			throw std::runtime_error{"malformed REGEX_RAW packet"};

		if (response.regex == nullptr)
			throw std::runtime_error{"misplaced REGEX_RAW packet"};

		if (response.regex_raw)
			throw std::runtime_error{"duplicate REGEX_RAW packet"};

		response.regex_raw = true;
		return;
#else
		break;
#endif

	case TranslationCommand::ALLOW_REMOTE_NETWORK:
#if TRANSLATION_ENABLE_HTTP
		HandleAllowRemoteNetwork(payload);
		return;
#else
		break;
#endif

	case TranslationCommand::RATE_LIMIT_SITE_REQUESTS:
#if TRANSLATION_ENABLE_HTTP
		if (response.site == nullptr)
			throw std::runtime_error{"misplaced RATE_LIMIT_SITE_REQUESTS packet"};

		HandleTokenBucketParams(response.rate_limit_site_requests,
					"RATE_LIMIT_SITE_REQUESTS", payload);
		return;
#else
		break;
#endif

	case TranslationCommand::ACCEPT_HTTP:
#if TRANSLATION_ENABLE_HTTP
		if (!payload.empty())
			throw std::runtime_error("malformed ACCEPT_HTTP packet");

		if (response.accept_http)
			throw std::runtime_error("duplicate ACCEPT_HTTP packet");

		response.accept_http = true;
		return;
#else
		break;
#endif

	case TranslationCommand::CAP_SYS_RESOURCE:
#if TRANSLATION_ENABLE_SPAWN && defined(HAVE_LIBCAP)
		if (!payload.empty())
			throw std::runtime_error("malformed CAP_SYS_RESOURCE packet");

		if (auto &options = MakeChildOptions("misplaced CAP_SYS_RESOURCE packet");
		    options.cap_sys_resource)
			throw std::runtime_error("duplicate CAP_SYS_RESOURCE packet");
		else
			options.cap_sys_resource = true;
		return;
#else
		break;
#endif

	case TranslationCommand::CHROOT:
#if TRANSLATION_ENABLE_SPAWN
		if (!IsValidAbsolutePath(string_payload))
			throw std::runtime_error("malformed CHROOT packet");

		if (auto &options = MakeChildOptions("misplaced CHROOT packet");
		    options.chroot != nullptr)
			throw std::runtime_error("misplaced CHROOT packet");
		else
			options.chroot = string_payload.data();
		return;
#else
		break;
#endif

	case TranslationCommand::TMPFS_DIRS_READABLE:
#if TRANSLATION_ENABLE_SPAWN
		if (!payload.empty())
			throw std::runtime_error("malformed TMPFS_DIRS_READABLE packet");

		if (auto &options = MakeMountNamespaceOptions("misplaced TMPFS_DIRS_READABLE packet");
		    !options.IsEnabled())
			throw std::runtime_error("misplaced TMPFS_DIRS_READABLE packet");
		else
			options.dir_mode = 0755;
		return;
#else
		break;
#endif

	case TranslationCommand::SYMLINK:
#if TRANSLATION_ENABLE_SPAWN
		previous_command = command;
		HandleSymlink(string_payload);
		return;
#else
		break;
#endif

	case TranslationCommand::BIND_MOUNT_RW_EXEC:
#if TRANSLATION_ENABLE_SPAWN
		previous_command = command;
		HandleBindMount(string_payload, false, true, true);
		return;
#else
		break;
#endif

	case TranslationCommand::BIND_MOUNT_FILE_EXEC:
#if TRANSLATION_ENABLE_SPAWN
		previous_command = command;
		HandleBindMount(string_payload, false, false, true, true);
		return;
#else
		break;
#endif

	case TranslationCommand::REAL_UID_GID:
#if TRANSLATION_ENABLE_SPAWN
		HandleRealUidGid(payload);
		return;
#else
		break;
#endif

	case TranslationCommand::RATE_LIMIT_SITE_TRAFFIC:
#if TRANSLATION_ENABLE_HTTP
		if (response.site == nullptr)
			throw std::runtime_error{"misplaced RATE_LIMIT_SITE_TRAFFIC packet"};

		HandleTokenBucketParams(response.rate_limit_site_traffic,
					"RATE_LIMIT_SITE_TRAFFIC", payload);
		return;
#else
		break;
#endif

	case TranslationCommand::ARCH:
		response.arch = ParseArch(string_payload);
		if (response.arch == Arch::NONE)
			throw std::runtime_error{"malformed ARCH packet"};

		return;
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
#if TRANSLATION_ENABLE_SPAWN
#if TRANSLATION_ENABLE_EXECUTE
		execute_options = nullptr;
#endif
		child_options = nullptr;
#endif
#if TRANSLATION_ENABLE_RADDRESS
		file_address = nullptr;
		http_address = nullptr;
		cgi_address = nullptr;
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
