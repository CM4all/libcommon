// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Parser.hxx"
#include "Response.hxx"
#include "String.hxx"
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
		throw TranslateParser::MalformedPacket{};

	for (const auto &packet : payload) {
		if (packet.group < int(HeaderGroup::ALL) ||
		    packet.group >= int(HeaderGroup::MAX) ||
		    (packet.mode != unsigned(HeaderForwardMode::NO) &&
		     packet.mode != unsigned(HeaderForwardMode::YES) &&
		     packet.mode != unsigned(HeaderForwardMode::BOTH) &&
		     packet.mode != unsigned(HeaderForwardMode::MANGLE)) ||
		    packet.reserved != 0)
			throw TranslateParser::MalformedPacket{};

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
		throw TranslateParser::MalformedPacket{};

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
#if TRANSLATION_ENABLE_HTTP
			std::span<const MaskedInetAddress> allow_remote_networks,
#endif
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

	if (!allow_remote_networks.empty())
		response.allow_remote_networks = NetworkList{alloc.Dup(allow_remote_networks)};
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
translate_client_pair_valid(std::string_view payload) noexcept
{
	return !payload.empty() && payload.front() != '=' &&
		IsValidString(payload) &&
		payload.substr(1).find('=') != payload.npos;
}

static void
translate_client_check_pair(std::string_view payload)
{
	if (!translate_client_pair_valid(payload))
		throw TranslateParser::MalformedPacket{};
}

static void
translate_client_pair(AllocatorPtr alloc,
		      ExpandableStringList::Builder &builder,
		      std::string_view payload)
{
	translate_client_check_pair(payload);

	builder.Add(alloc, payload.data(), false);
}

#endif // TRANSLATION_ENABLE_SPAWN

#if TRANSLATION_ENABLE_EXPAND

static void
translate_client_expand_pair(ExpandableStringList::Builder &builder,
			     std::string_view payload)
{
	if (!builder.CanSetExpand())
		throw TranslateParser::MisplacedPacket{};

	translate_client_check_pair(payload);

	builder.SetExpand(payload.data());
}

#endif

#if TRANSLATION_ENABLE_SPAWN

inline void
TranslateParser::HandlePivotRoot(std::string_view payload)
{
	if (!IsValidAbsolutePath(payload))
		throw MalformedPacket{};

	auto &options = MakeMountNamespaceOptions("misplaced PIVOT_ROOT packet");

	if (options.pivot_root != nullptr ||
	    options.mount_root_tmpfs)
		throw DuplicatePacket{};

	options.pivot_root = payload.data();
}

inline void
TranslateParser::HandleMountRootTmpfs(std::string_view payload)
{
	if (!payload.empty())
		throw MalformedPacket{};

	auto &options = MakeMountNamespaceOptions("misplaced MOUNT_ROOT_TMPFS packet");

	if (options.pivot_root != nullptr ||
	    options.mount_root_tmpfs)
		throw DuplicatePacket{};

	options.mount_root_tmpfs = true;
}

inline void
TranslateParser::HandleHome(std::string_view payload)
{
	if (!IsValidAbsolutePath(payload))
		throw MalformedPacket{};

	auto &options = MakeMountNamespaceOptions("misplaced HOME packet");

	if (options.home != nullptr)
		throw DuplicatePacket{};

	options.home = payload.data();
}

#if TRANSLATION_ENABLE_EXPAND

inline void
TranslateParser::HandleExpandHome(std::string_view payload)
{
	if (!IsValidAbsolutePath(payload))
		throw MalformedPacket{};

	auto &options = MakeMountNamespaceOptions("misplaced EXPAND_HOME packet");
	if (options.expand_home)
		throw DuplicatePacket{};

	options.expand_home = true;
	options.home = payload.data();
}

#endif

inline void
TranslateParser::HandleMountProc(std::string_view payload)
{
	if (!payload.empty())
		throw MalformedPacket{};

	auto &options = MakeMountNamespaceOptions("misplaced MOUNT_PROC packet");
	if (options.mount_proc)
		throw DuplicatePacket{};

	options.mount_proc = true;
}

inline void
TranslateParser::HandleMountTmpTmpfs(std::string_view payload)
{
	if (!IsValidString(payload))
		throw MalformedPacket{};

	auto &options = MakeMountNamespaceOptions("misplaced MOUNT_TMP_TMPFS packet");

	if (options.mount_tmp_tmpfs != nullptr)
		throw DuplicatePacket{};

	options.mount_tmp_tmpfs = payload.data() != nullptr
		? payload.data()
		: "";
}

inline void
TranslateParser::HandleMountTmpTmpfsExec(std::string_view payload)
{
	if (!payload.empty())
		throw MalformedPacket{};

	auto &options = MakeMountNamespaceOptions("misplaced MOUNT_TMP_TMPFS_EXEC packet");

	if (options.mount_tmp_tmpfs == nullptr)
		throw MisplacedPacket{};

	if (options.mount_tmp_tmpfs_exec)
		throw DuplicatePacket{};

	options.mount_tmp_tmpfs_exec = true;
}

inline void
TranslateParser::HandleMountHome(std::string_view payload)
{
	if (!IsValidAbsolutePath(payload))
		throw MalformedPacket{};

	auto &options = MakeMountNamespaceOptions("misplaced RLIMITS packet");
	if (options.home == nullptr)
		throw MisplacedPacket{};

	if (options.HasMountOn(payload.data()))
		throw DuplicatePacket{};

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
		throw MalformedPacket{};

	AddMount("misplaced MOUNT_TMPFS packet",
		 alloc.New<Mount>(Mount::Tmpfs{}, payload.data(), writable));
}

inline void
TranslateParser::HandleMountNamedTmpfs(std::string_view payload)
{
	const auto [source, target] = Split(payload, '\0');
	if (!IsValidName(source) ||
	    !IsValidAbsolutePath(target))
		throw MalformedPacket{};

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
		throw MalformedPacket{};

	const char *source;
	if (SkipPrefix(_source, "container:"sv)) {
		/* path on host (host's mount namespace) */

		if (!IsValidAbsolutePath(_source))
			throw MalformedPacket{};

		/* keep the slash */
		source = _source.data();
	} else {
		/* no prefix or prefix "host:": path on host (host's
		   mount namespace) */
		SkipPrefix(_source, "host:"sv);

		if (!IsValidAbsolutePath(_source))
			throw MalformedPacket{};

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
		throw MalformedPacket{};

	auto *m = alloc.New<Mount>(target.data(), linkpath.data());
	m->type = Mount::Type::SYMLINK;
	AddMount("misplaced SYMLINK packet", m);
}

inline void
TranslateParser::HandleWriteFile(std::string_view payload)
{
	const auto [path, contents] = Split(payload, '\0');
	if (!IsValidAbsolutePath(path) || !IsValidString(contents))
		throw MalformedPacket{};

	auto *m = alloc.New<Mount>(Mount::WriteFile{},
				   path.data(),
				   contents.data());

	AddMount("misplaced WRITE_FILE packet", m);
}

inline void
TranslateParser::HandleUtsNamespace(std::string_view payload)
{
	if (!IsValidNonEmptyString(payload))
		throw MalformedPacket{};

	auto &options = MakeNamespaceOptions("misplaced UTS_NAMESPACE packet");
	if (options.hostname != nullptr)
		throw MisplacedPacket{};

	options.hostname = payload.data();
}

inline void
TranslateParser::HandleRlimits(std::string_view payload)
{
	auto &options = MakeChildOptions("misplaced RLIMITS packet");

	if (options.rlimits == nullptr)
		options.rlimits = alloc.New<ResourceLimits>();

	if (!options.rlimits->Parse(payload))
		throw MalformedPacket{};
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
		throw DuplicatePacket{};

	if (payload_length % sizeof(payload[0]) != 0)
		throw MalformedPacket{};

	response.want = { payload, payload_length / sizeof(payload[0]) };
}

#endif

#if TRANSLATION_ENABLE_RADDRESS

static void
translate_client_file_not_found(TranslateResponse &response,
				std::span<const std::byte> payload)
{
	if (response.file_not_found.data() != nullptr)
		throw TranslateParser::DuplicatePacket{};

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
		throw MisplacedPacket{};

	if (content_type_lookup->data() != nullptr)
		throw DuplicatePacket{};

	if (content_type != nullptr)
		throw std::runtime_error("CONTENT_TYPE/CONTENT_TYPE_LOOKUP conflict");

	*content_type_lookup = payload;
}

static void
translate_client_enotdir(TranslateResponse &response,
			 std::span<const std::byte> payload)
{
	if (response.enotdir.data() != nullptr)
		throw TranslateParser::DuplicatePacket{};

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
		throw TranslateParser::DuplicatePacket{};

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
		throw TranslateParser::DuplicatePacket{};

	if (payload.size() != sizeof(uint32_t))
		throw TranslateParser::MalformedPacket{};

	response.expires_relative = std::chrono::seconds(*(const uint32_t *)(const void *)payload.data());
}

static void
translate_client_expires_relative_with_query(TranslateResponse &response,
					     std::span<const std::byte> payload)
{
	if (response.expires_relative_with_query > std::chrono::seconds::zero())
		throw TranslateParser::DuplicatePacket{};

	if (payload.size() != sizeof(uint32_t))
		throw TranslateParser::MalformedPacket{};

	response.expires_relative_with_query = std::chrono::seconds(*(const uint32_t *)(const void *)payload.data());
}

#if TRANSLATION_ENABLE_SPAWN

inline void
TranslateParser::HandleStderrPath(std::string_view payload, bool jailed)
{
	if (!IsValidAbsolutePath(payload))
		throw MalformedPacket{};

	auto &options = MakeChildOptions("misplaced STDERR_PATH packet");

	if (options.stderr_null)
		throw MisplacedPacket{};

	if (options.stderr_path != nullptr)
		throw DuplicatePacket{};

	options.stderr_path = payload.data();
	options.stderr_jailed = jailed;
}

#if TRANSLATION_ENABLE_EXPAND

inline void
TranslateParser::HandleExpandStderrPath(std::string_view payload)
{
	if (!IsValidNonEmptyString(payload))
		throw MalformedPacket{};

	auto &options = MakeChildOptions("misplaced EXPAND_STDERR_PATH packet");

	if (options.expand_stderr_path != nullptr)
		throw DuplicatePacket{};

	options.expand_stderr_path = payload.data();
}

#endif

inline void
TranslateParser::HandleUidGid(std::span<const std::byte> _payload)
{
	auto &uid_gid = MakeChildOptions("misplaced RLIMITS packet").uid_gid;

	if (!uid_gid.IsEmpty())
		throw DuplicatePacket{};

	constexpr size_t min_size = sizeof(int) * 2;
	const size_t max_size = min_size + sizeof(int) * uid_gid.supplementary_groups.max_size();

	if (_payload.size() < min_size || _payload.size() > max_size ||
	    _payload.size() % sizeof(int) != 0)
		throw MalformedPacket{};

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
	    !options.ns.user.create)
		throw MisplacedPacket{};

	const auto *value = (const uint32_t *)(const void *)payload.data();
	if (payload.size() != sizeof(*value) || *value <= 0)
		throw MalformedPacket{};

	if (options.ns.user.mapped_effective_uid != 0)
		throw DuplicatePacket{};

	options.ns.user.mapped_effective_uid = *value;
}

inline void
TranslateParser::HandleMappedRealUidGid(std::span<const std::byte> payload)
{
	auto &options = MakeChildOptions("misplaced MAPPED_REAL_UID_GID packet");

	if (options.uid_gid.real_uid == UidGid::UNSET_UID ||
	    !options.ns.user.create)
		throw MisplacedPacket{};

	const auto *value = (const uint32_t *)(const void *)payload.data();
	if (payload.size() != sizeof(*value) || *value <= 0)
		throw MalformedPacket{};

	if (options.ns.user.mapped_real_uid != 0)
		throw DuplicatePacket{};

	options.ns.user.mapped_real_uid = *value;
}

inline void
TranslateParser::HandleRealUidGid(std::span<const std::byte> payload)
{
	auto &options = MakeChildOptions("misplaced REAL_UID packet");

	if (options.uid_gid.IsEmpty())
		throw MisplacedPacket{};

	if (options.uid_gid.HasReal())
		throw DuplicatePacket{};

	if (payload.size() < sizeof(options.uid_gid.real_uid))
		throw MalformedPacket{};

	LoadUnaligned(options.uid_gid.real_uid, payload.data());
	payload = payload.subspan(sizeof(options.uid_gid.real_uid));

	if (payload.size() >= sizeof(options.uid_gid.real_gid)) {
		LoadUnaligned(options.uid_gid.real_gid, payload.data());
		payload = payload.subspan(sizeof(options.uid_gid.real_gid));
	}

	if (!payload.empty())
		throw MalformedPacket{};

	if (!options.uid_gid.HasReal())
		throw MalformedPacket{};
}

inline void
TranslateParser::HandleMaxInotify(std::span<const std::byte> payload)
{
	auto &ns = MakeNamespaceOptions("misplaced MAX_INOTIFY packet");
	if (!ns.user.create)
		throw MisplacedPacket{};

	if (payload.size() != sizeof(uint32_t) * 2)
		throw MalformedPacket{};

	const std::byte *src = payload.data();

	ns.user.limits.max_inotify_instances = LoadUnaligned<uint32_t>(src);
	src += sizeof(uint32_t);

	ns.user.limits.max_inotify_watches = LoadUnaligned<uint32_t>(src);
}

inline void
TranslateParser::HandleUmask(std::span<const std::byte> payload)
{
	typedef uint16_t value_type;

	auto &options = MakeChildOptions("misplaced UMASK packet");

	if (options.umask >= 0)
		throw DuplicatePacket{};

	if (payload.size() != sizeof(value_type))
		throw MalformedPacket{};

	auto umask = *(const uint16_t *)(const void *)payload.data();
	if (umask & ~0777)
		throw MalformedPacket{};

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
		throw MalformedPacket{};

	options.cgroup.Set(alloc, set.first, set.second);
}

inline void
TranslateParser::HandleCgroupXattr(std::string_view payload)
{
	auto &options = MakeChildOptions("misplaced CGROUP_XATTR packet");

	auto xattr = ParseCgroupSet(payload);
	if (xattr.first.data() == nullptr)
		throw MalformedPacket{};

	options.cgroup.SetXattr(alloc, xattr.first, xattr.second);
}

inline void
TranslateParser::HandleProcessName(std::string_view payload)
{
	if (!IsValidNonEmptyString(payload))
		throw MalformedPacket{};

	if (cgi_address != nullptr) {
		if (cgi_address->process_name != nullptr)
			throw DuplicatePacket{};

		cgi_address->process_name = payload.data();
	} else if (lhttp_address != nullptr) {
		if (lhttp_address->process_name != nullptr)
			throw DuplicatePacket{};

		lhttp_address->process_name = payload.data();
	} else
		throw MisplacedPacket{};
}

inline void
TranslateParser::HandleMountListenStream(std::span<const std::byte> payload)
{
	auto &options = MakeMountNamespaceOptions("misplaced MOUNT_LISTEN_STREAM packet");

	if (options.mount_listen_stream.data() != nullptr)
		throw DuplicatePacket{};

	const auto [path, rest] = Split(ToStringView(payload), '\0');
	if (!IsValidAbsolutePath(path))
		throw MalformedPacket{};

	options.mount_listen_stream = payload;
}

#endif // TRANSLATION_ENABLE_SPAWN

#if TRANSLATION_ENABLE_HTTP

inline void
TranslateParser::HandleAllowRemoteNetwork(std::span<const std::byte> payload)
{
	if (payload.size() < 2)
		throw MalformedPacket{};

	const uint_least8_t prefix_length = static_cast<uint8_t>(payload.front());
	const SocketAddress address{
		reinterpret_cast<const struct sockaddr *>(payload.data() + 1),
		static_cast<SocketAddress::size_type>(payload.size() - 1),
	};

	MaskedInetAddress inet_address;
	if (!inet_address.CopyFrom(address, prefix_length))
		throw MalformedPacket{};

	allow_remote_networks_builder.emplace_back(inet_address);
}

inline void
TranslateParser::HandleTokenBucketParams(TranslateTokenBucketParams &params,
					 std::span<const std::byte> payload)
{
	if (params.IsDefined())
		throw DuplicatePacket{};

	if (payload.size() != sizeof(params))
		throw MalformedPacket{};

	memcpy(&params, payload.data(), sizeof(params));
	if (!params.IsValid())
		throw MalformedPacket{};
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
	case TranslationCommand::PEEK:
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
			throw MalformedPacket{};

		static_assert(sizeof(HttpStatus) == 2);
		response.status = *(const HttpStatus *)(const void *)payload.data();

#if TRANSLATION_ENABLE_HTTP
		if (!http_status_is_valid(response.status))
			throw FmtRuntimeError("invalid HTTP status code {}",
					      std::to_underlying(response.status));
#endif

		return;

	case TranslationCommand::PATH:
#if TRANSLATION_ENABLE_RADDRESS
		if (!IsValidAbsolutePath(string_payload))
			throw MalformedPacket{};

		if (resource_address == nullptr || resource_address->IsDefined())
			throw MisplacedPacket{};

		file_address = alloc.New<FileAddress>(string_payload.data());
		*resource_address = *file_address;
		return;
#else
		break;
#endif

	case TranslationCommand::PATH_INFO:
#if TRANSLATION_ENABLE_RADDRESS
		if (!IsValidString(string_payload))
			throw MalformedPacket{};

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
			throw MisplacedPacket{};
#else
		break;
#endif

	case TranslationCommand::EXPAND_PATH:
#if TRANSLATION_ENABLE_RADDRESS && TRANSLATION_ENABLE_EXPAND
		if (!IsValidString(string_payload))
			throw MalformedPacket{};

		if (response.regex == nullptr) {
			throw MisplacedPacket{};
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
			throw MisplacedPacket{};
#else
		break;
#endif

	case TranslationCommand::EXPAND_PATH_INFO:
#if TRANSLATION_ENABLE_RADDRESS && TRANSLATION_ENABLE_EXPAND
		if (!IsValidString(string_payload))
			throw MalformedPacket{};

		if (response.regex == nullptr) {
			throw MisplacedPacket{};
		} else if (cgi_address != nullptr &&
			   !cgi_address->expand_path_info) {
			cgi_address->path_info = string_payload.data();
			cgi_address->expand_path_info = true;
		} else if (file_address != nullptr) {
			/* don't emit an error when the resource is a local path.
			   This combination might be useful one day, but isn't
			   currently used. */
		} else
			throw MisplacedPacket{};

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
			throw MalformedPacket{};

		if (file_address != nullptr) {
			if (file_address->auto_gzipped ||
			    file_address->gzipped != nullptr)
				throw DuplicatePacket{};

			file_address->gzipped = string_payload.data();
			return;
		} else {
			throw MisplacedPacket{};
		}
#else
		break;
#endif

	case TranslationCommand::SITE:
#if TRANSLATION_ENABLE_RADDRESS
		assert(resource_address != nullptr);

		if (!IsValidNonEmptyString(string_payload))
			throw MalformedPacket{};

		if (resource_address == &response.address)
#endif
			response.site = string_payload.data();
#if TRANSLATION_ENABLE_RADDRESS
		else
			throw MisplacedPacket{};
#endif

		return;

	case TranslationCommand::CONTENT_TYPE:
#if TRANSLATION_ENABLE_RADDRESS
		if (!IsValidNonEmptyString(string_payload))
			throw MalformedPacket{};

		if (file_address != nullptr) {
			if (file_address->content_type_lookup.data() != nullptr)
				throw std::runtime_error("CONTENT_TYPE/CONTENT_TYPE_LOOKUP conflict");

			file_address->content_type = string_payload.data();
		} else if (from_request.content_type_lookup) {
			response.content_type = string_payload.data();
		} else
			throw MisplacedPacket{};

		return;
#else
		break;
#endif

	case TranslationCommand::HTTP:
#if TRANSLATION_ENABLE_RADDRESS
		if (resource_address == nullptr || resource_address->IsDefined())
			throw MisplacedPacket{};

		if (!IsValidNonEmptyString(string_payload))
			throw MalformedPacket{};

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
			throw MalformedPacket{};

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
			throw MisplacedPacket{};

		if (!IsValidNonEmptyString(string_payload))
			throw MalformedPacket{};

		response.redirect = string_payload.data();
		response.expand_redirect = true;
		return;
#else
		break;
#endif

	case TranslationCommand::BOUNCE:
#if TRANSLATION_ENABLE_HTTP
		if (!IsValidNonEmptyString(string_payload))
			throw MalformedPacket{};

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
			throw MisplacedPacket{};

		transformation->u.processor.options |= PROCESSOR_CONTAINER;
		return;
#else
		break;
#endif

	case TranslationCommand::SELF_CONTAINER:
#if TRANSLATION_ENABLE_TRANSFORMATION
		if (transformation == nullptr ||
		    transformation->type != Transformation::Type::PROCESS)
			throw MisplacedPacket{};

		transformation->u.processor.options |=
			PROCESSOR_SELF_CONTAINER|PROCESSOR_CONTAINER;
		return;
#else
		break;
#endif

	case TranslationCommand::GROUP_CONTAINER:
#if TRANSLATION_ENABLE_TRANSFORMATION
		if (!IsValidNonEmptyString(string_payload))
			throw MalformedPacket{};

		if (transformation == nullptr ||
		    transformation->type != Transformation::Type::PROCESS)
			throw MisplacedPacket{};

		transformation->u.processor.options |= PROCESSOR_CONTAINER;
		response.container_groups.Add(alloc, string_payload.data());
		return;
#else
		break;
#endif

	case TranslationCommand::WIDGET_GROUP:
#if TRANSLATION_ENABLE_WIDGET
		if (!IsValidNonEmptyString(string_payload))
			throw MalformedPacket{};

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
			throw MalformedPacket{};

		if (response.HasUntrusted())
			throw MisplacedPacket{};

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
			throw MalformedPacket{};

		if (response.HasUntrusted())
			throw MisplacedPacket{};

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
			throw MalformedPacket{};

		if (response.HasUntrusted())
			throw MisplacedPacket{};

		response.untrusted_site_suffix = string_payload.data();
		return;
#else
		break;
#endif

	case TranslationCommand::SCHEME:
#if TRANSLATION_ENABLE_HTTP
		if (!string_payload.starts_with("http"sv))
			throw MalformedPacket{};

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
#if TRANSLATION_ENABLE_RADDRESS
		if (response.layout.data() != nullptr) {
			assert(layout_items_builder);

			layout_items_builder->emplace_back(TranslationLayoutItem::Type::EXACT,
							   string_payload,
							   pcre_cache);
			return;
		}
#endif

		if (!IsValidAbsoluteUriPath(string_payload))
			throw MalformedPacket{};

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
			throw MalformedPacket{};

		if (response.realm != nullptr)
			throw DuplicatePacket{};

		if (response.realm_from_auth_base)
			throw MisplacedPacket{};

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
			throw MisplacedPacket{};

		if (payload.empty())
			throw MalformedPacket{};

		SetCgiAddress(ResourceAddress::Type::PIPE, string_payload.data());
		return;
#else
		break;
#endif

	case TranslationCommand::CGI:
#if TRANSLATION_ENABLE_RADDRESS
		if (resource_address == nullptr || resource_address->IsDefined())
			throw MisplacedPacket{};

		if (!IsValidAbsolutePath(string_payload))
			throw MalformedPacket{};

		SetCgiAddress(ResourceAddress::Type::CGI, string_payload.data());
		cgi_address->document_root = response.document_root;
		return;
#else
		break;
#endif

	case TranslationCommand::FASTCGI:
#if TRANSLATION_ENABLE_RADDRESS
		if (resource_address == nullptr || resource_address->IsDefined())
			throw MisplacedPacket{};

		if (!IsValidAbsolutePath(string_payload))
			throw MalformedPacket{};

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
			throw MisplacedPacket{};

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
			throw MisplacedPacket{};

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
			throw MisplacedPacket{};

		cgi_address->script_name = string_payload.data();
		return;
#else
		break;
#endif

	case TranslationCommand::EXPAND_SCRIPT_NAME:
#if TRANSLATION_ENABLE_RADDRESS && TRANSLATION_ENABLE_EXPAND
		if (!IsValidNonEmptyString(string_payload))
			throw MalformedPacket{};

		if (response.regex == nullptr ||
		    cgi_address == nullptr ||
		    cgi_address->expand_script_name)
			throw MisplacedPacket{};

		cgi_address->script_name = string_payload.data();
		cgi_address->expand_script_name = true;
		return;
#else
		break;
#endif

	case TranslationCommand::DOCUMENT_ROOT:
#if TRANSLATION_ENABLE_RADDRESS
		if (!IsValidAbsolutePath(string_payload))
			throw MalformedPacket{};

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
			throw MalformedPacket{};

		if (response.regex == nullptr)
			throw MisplacedPacket{};

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
			throw MisplacedPacket{};

		if (payload.size() < 2)
			throw MalformedPacket{};

		if (const SocketAddress address{payload}; address.IsValid())
			address_list_builder.Add(alloc, SocketAddress{payload});
		else
			throw MalformedPacket{};

		return;
#else
		break;
#endif

	case TranslationCommand::ADDRESS_STRING:
#if TRANSLATION_ENABLE_HTTP
		if (address_list == nullptr)
			throw MisplacedPacket{};

		if (!IsValidNonEmptyString(string_payload))
			throw MalformedPacket{};

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
			throw MalformedPacket{};

		AddView(string_payload.data());
		return;
#else
		break;
#endif

	case TranslationCommand::MAX_AGE:
		if (payload.size() != 4)
			throw MalformedPacket{};

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
			throw MisplacedPacket{};
		}

		return;

	case TranslationCommand::VARY:
#if TRANSLATION_ENABLE_CACHE
		if (payload.empty() ||
		    payload.size() > 16 * sizeof(response.vary.front()) ||
		    payload.size() % sizeof(response.vary.front()) != 0)
			throw MalformedPacket{};

		response.vary = FromBytesFloor<const TranslationCommand>(payload);
#endif
		return;

	case TranslationCommand::INVALIDATE:
#if TRANSLATION_ENABLE_CACHE
		if (payload.empty() ||
		    payload.size() > 16 * sizeof(response.invalidate.front()) ||
		    payload.size() % sizeof(response.invalidate.front()) != 0)
			throw MalformedPacket{};

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
			throw MalformedPacket{};

		if (response.layout.data() != nullptr) {
			assert(layout_items_builder);

			layout_items_builder->emplace_back(TranslationLayoutItem::Type::BASE,
							   string_payload,
							   pcre_cache);
			return;
		}

		if (from_request.uri == nullptr ||
		    response.auto_base ||
		    response.base != nullptr)
			throw MisplacedPacket{};

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
			throw MalformedPacket{};

		if (response.base == nullptr)
			throw MisplacedPacket{};

		if (response.unsafe_base)
			throw DuplicatePacket{};

		response.unsafe_base = true;
		return;
#else
		break;
#endif

	case TranslationCommand::EASY_BASE:
#if TRANSLATION_ENABLE_RADDRESS
		if (!payload.empty())
			throw MalformedPacket{};

		if (response.base == nullptr)
			throw std::runtime_error("EASY_BASE without BASE");

		if (response.easy_base)
			throw DuplicatePacket{};

		response.easy_base = true;
		return;
#else
		break;
#endif

	case TranslationCommand::REGEX:
#if TRANSLATION_ENABLE_EXPAND
		if (!IsValidNonEmptyString(string_payload))
			throw MalformedPacket{};

		if (response.layout.data() != nullptr) {
			assert(layout_items_builder);

			layout_items_builder->emplace_back(TranslationLayoutItem::Type::REGEX,
							   string_payload,
							   pcre_cache);
			return;
		}

		if (response.base == nullptr)
			throw std::runtime_error("REGEX without BASE");

		if (response.regex != nullptr)
			throw DuplicatePacket{};

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
			throw DuplicatePacket{};

		if (!IsValidNonEmptyString(string_payload))
			throw MalformedPacket{};

		response.inverse_regex = string_payload.data();
		return;
#else
		break;
#endif

	case TranslationCommand::REGEX_TAIL:
#if TRANSLATION_ENABLE_EXPAND
		if (!payload.empty())
			throw MalformedPacket{};

		if (response.regex == nullptr &&
		    response.inverse_regex == nullptr &&
		    response.layout.data() == nullptr)
			throw MisplacedPacket{};

		if (response.regex_tail)
			throw DuplicatePacket{};

		response.regex_tail = true;
		return;
#else
		break;
#endif

	case TranslationCommand::REGEX_UNESCAPE:
#if TRANSLATION_ENABLE_EXPAND
		if (!payload.empty())
			throw MalformedPacket{};

		if (response.regex == nullptr && response.inverse_regex == nullptr)
			throw MisplacedPacket{};

		if (response.regex_unescape)
			throw DuplicatePacket{};

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
			throw MalformedPacket{};

		if (!HasArgs())
			throw MisplacedPacket{};

		args_builder.Add(alloc, string_payload.data(), false);
		return;
#else
		break;
#endif

	case TranslationCommand::EXPAND_APPEND:
#if TRANSLATION_ENABLE_EXPAND
		if (!IsValidNonEmptyString(string_payload))
			throw MalformedPacket{};

		if (response.regex == nullptr || !HasArgs() ||
		    !args_builder.CanSetExpand())
			throw MisplacedPacket{};

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
			translate_client_pair(alloc, params_builder,
					      string_payload);
			return;
		}
#endif

#if TRANSLATION_ENABLE_SPAWN
		MakeChildOptions("misplaced PAIR packet");
		translate_client_pair(alloc, env_builder,
				      string_payload);
		return;
#else
		break;
#endif // TRANSLATION_ENABLE_SPAWN

	case TranslationCommand::EXPAND_PAIR:
#if TRANSLATION_ENABLE_RADDRESS
		if (response.regex == nullptr)
			throw MisplacedPacket{};

		if (cgi_address != nullptr) {
			const auto type = resource_address->type;
			auto &builder = type == ResourceAddress::Type::CGI
				? env_builder
				: params_builder;

			translate_client_expand_pair(builder, string_payload);
		} else if (lhttp_address != nullptr) {
			translate_client_expand_pair(env_builder, string_payload);
		} else
			throw MisplacedPacket{};
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
			throw MalformedPacket{};

		response.www_authenticate = string_payload.data();
		return;
#else
		break;
#endif

	case TranslationCommand::AUTHENTICATION_INFO:
#if TRANSLATION_ENABLE_SESSION
		if (!IsValidNonEmptyString(string_payload))
			throw MalformedPacket{};

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
			throw MisplacedPacket{};

		if (!IsValidNonEmptyString(string_payload))
			throw MalformedPacket{};

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
			throw DuplicatePacket{};

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
			throw MisplacedPacket{};

		if (!IsValidAbsolutePath(string_payload))
			throw MalformedPacket{};

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
			throw MisplacedPacket{};

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
			throw MisplacedPacket{};

		if (!IsValidNonEmptyString(string_payload))
			throw MalformedPacket{};

		response.cookie_host = string_payload.data();
		return;
#else
		break;
#endif

	case TranslationCommand::COOKIE_PATH:
#if TRANSLATION_ENABLE_SESSION
		if (response.cookie_path != nullptr)
			throw MisplacedPacket{};

		if (!IsValidAbsoluteUriPath(string_payload))
			throw MalformedPacket{};

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
			throw MisplacedPacket{};

		switch (transformation->type) {
		case Transformation::Type::PROCESS:
			transformation->u.processor.options |= PROCESSOR_PREFIX_CSS_CLASS;
			break;

		case Transformation::Type::PROCESS_CSS:
			transformation->u.css_processor.options |= CSS_PROCESSOR_PREFIX_CLASS;
			break;

		default:
			throw MisplacedPacket{};
		}

		return;
#else
		break;
#endif

	case TranslationCommand::PREFIX_XML_ID:
#if TRANSLATION_ENABLE_TRANSFORMATION
		if (transformation == nullptr)
			throw MisplacedPacket{};

		switch (transformation->type) {
		case Transformation::Type::PROCESS:
			transformation->u.processor.options |= PROCESSOR_PREFIX_XML_ID;
			break;

		case Transformation::Type::PROCESS_CSS:
			transformation->u.css_processor.options |= CSS_PROCESSOR_PREFIX_ID;
			break;

		default:
			throw MisplacedPacket{};
		}

		return;
#else
		break;
#endif

	case TranslationCommand::PROCESS_STYLE:
#if TRANSLATION_ENABLE_TRANSFORMATION
		if (transformation == nullptr ||
		    transformation->type != Transformation::Type::PROCESS)
			throw MisplacedPacket{};

		transformation->u.processor.options |= PROCESSOR_STYLE;
		return;
#else
		break;
#endif

	case TranslationCommand::FOCUS_WIDGET:
#if TRANSLATION_ENABLE_TRANSFORMATION
		if (transformation == nullptr ||
		    transformation->type != Transformation::Type::PROCESS)
			throw MisplacedPacket{};

		transformation->u.processor.options |= PROCESSOR_FOCUS_WIDGET;
		return;
#else
		break;
#endif

	case TranslationCommand::ANCHOR_ABSOLUTE:
#if TRANSLATION_ENABLE_WIDGET
		if (transformation == nullptr ||
		    transformation->type != Transformation::Type::PROCESS)
			throw MisplacedPacket{};

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
			throw MisplacedPacket{};

		if (string_payload.empty() || string_payload.back() != '/')
			throw MalformedPacket{};

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
			throw MisplacedPacket{};

		response.auto_base = true;
		return;
#else
		break;
#endif

	case TranslationCommand::VALIDATE_MTIME:
		if (string_payload.size() < 10 || string_payload[8] != '/' ||
		    memchr(string_payload.data() + 9, 0,
			   string_payload.size() - 9) != nullptr)
			throw MalformedPacket{};

		response.validate_mtime.mtime = *(const uint64_t *)(const void *)payload.data();
		response.validate_mtime.path =
			alloc.DupZ(string_payload.substr(8));
		return;

	case TranslationCommand::LHTTP_PATH:
#if TRANSLATION_ENABLE_RADDRESS
		if (resource_address == nullptr || resource_address->IsDefined())
			throw MisplacedPacket{};

		if (!IsValidAbsolutePath(string_payload))
			throw MalformedPacket{};

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
			throw MisplacedPacket{};

		if (!IsValidAbsoluteUriPath(string_payload))
			throw MalformedPacket{};

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
			throw MisplacedPacket{};

		if (!IsValidNonEmptyString(string_payload))
			throw MalformedPacket{};

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
			throw MisplacedPacket{};

		if (!IsValidNonEmptyString(string_payload))
			throw MalformedPacket{};

		lhttp_address->host_and_port = string_payload.data();
		return;
#else
		break;
#endif

	case TranslationCommand::CONCURRENCY:
#if TRANSLATION_ENABLE_RADDRESS
		if (payload.size() != 2)
			throw MalformedPacket{};

		if (lhttp_address != nullptr)
			lhttp_address->concurrency = *(const uint16_t *)(const void *)payload.data();
		else if (cgi_address != nullptr)
			cgi_address->concurrency = *(const uint16_t *)(const void *)payload.data();
		else
			throw MisplacedPacket{};

		return;
#else
		break;
#endif

	case TranslationCommand::WANT_FULL_URI:
#if TRANSLATION_ENABLE_HTTP
		if (from_request.want_full_uri)
			throw std::runtime_error("WANT_FULL_URI loop");

		if (response.want_full_uri.data() != nullptr)
			throw DuplicatePacket{};

		response.want_full_uri = payload;
		return;
#else
		break;
#endif

	case TranslationCommand::USER_NAMESPACE:
#if TRANSLATION_ENABLE_SPAWN
		if (!payload.empty())
			throw MalformedPacket{};

		MakeNamespaceOptions("misplaced USER_NAMESPACE packet").user.create = true;
		return;
#else
		break;
#endif

	case TranslationCommand::PID_NAMESPACE:
#if TRANSLATION_ENABLE_SPAWN
		if (!payload.empty())
			throw MalformedPacket{};

		if (auto &options = MakeNamespaceOptions("misplaced PID_NAMESPACE packet");
		    options.pid.mode != PidNamespaceOptions::Mode::DISABLED)
			throw DuplicatePacket{};
		else
			options.pid.mode = PidNamespaceOptions::Mode::ANONYMOUS;

		return;
#else
		break;
#endif

	case TranslationCommand::NETWORK_NAMESPACE:
#if TRANSLATION_ENABLE_SPAWN
		if (!payload.empty())
			throw MalformedPacket{};

		if (auto &options = MakeNamespaceOptions("misplaced NETWORK_NAMESPACE packet");
		    options.enable_network)
			throw DuplicatePacket{};
		else if (options.network_namespace_name != nullptr)
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
			throw MalformedPacket{};

		if (response.test_path != nullptr)
			throw DuplicatePacket{};

		response.test_path = string_payload.data();
		return;

	case TranslationCommand::EXPAND_TEST_PATH:
#if TRANSLATION_ENABLE_EXPAND
		if (response.regex == nullptr)
			throw MisplacedPacket{};

		if (!IsValidNonEmptyString(string_payload))
			throw MalformedPacket{};

		if (response.expand_test_path)
			throw DuplicatePacket{};

		response.test_path = string_payload.data();
		response.expand_test_path = true;
		return;
#else
		break;
#endif

	case TranslationCommand::REDIRECT_QUERY_STRING:
#if TRANSLATION_ENABLE_HTTP
		if (!payload.empty())
			throw MalformedPacket{};

		if (response.redirect_query_string ||
		    response.redirect == nullptr)
			throw MisplacedPacket{};

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
			throw DuplicatePacket{};

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
				      string_payload);
		return;
#else
		break;
#endif

	case TranslationCommand::EXPAND_SETENV:
#if TRANSLATION_ENABLE_EXPAND
		if (response.regex == nullptr)
			throw MisplacedPacket{};

		translate_client_expand_pair(MakeEnvBuilder("misplaced EXPAND_SETENV packet"),
					     string_payload);
		return;
#else
		break;
#endif

	case TranslationCommand::EXPAND_URI:
#if TRANSLATION_ENABLE_EXPAND
		if (response.regex == nullptr ||
		    response.expand_uri)
			throw MisplacedPacket{};

		if (!IsValidNonEmptyString(string_payload))
			throw MalformedPacket{};

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
			throw MisplacedPacket{};

		if (!IsValidNonEmptyString(string_payload))
			throw MalformedPacket{};

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
			throw MisplacedPacket{};

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
			throw MalformedPacket{};

		if (file_address != nullptr) {
			if (file_address->auto_gzipped ||
			    file_address->gzipped != nullptr)
				throw DuplicatePacket{};

			file_address->auto_gzipped = true;
		} else if (from_request.content_type_lookup) {
			if (response.auto_gzipped)
				throw DuplicatePacket{};

			response.auto_gzipped = true;
		} else
			throw MisplacedPacket{};
#endif
		return;

	case TranslationCommand::PROBE_PATH_SUFFIXES:
		if (response.probe_path_suffixes.data() != nullptr ||
		    response.test_path == nullptr)
			throw MisplacedPacket{};

		response.probe_path_suffixes = payload;
		return;

	case TranslationCommand::PROBE_SUFFIX:
		if (response.probe_path_suffixes.data() == nullptr)
			throw MisplacedPacket{};

		if (probe_suffixes_builder.full())
			throw std::runtime_error("too many PROBE_SUFFIX packets");

		if (!CheckProbeSuffix(string_payload))
			throw MalformedPacket{};

		probe_suffixes_builder.push_back(string_payload.data());
		return;

	case TranslationCommand::AUTH_FILE:
#if TRANSLATION_ENABLE_SESSION
		if (response.HasAuth())
			throw DuplicatePacket{};

		if (!IsValidAbsolutePath(string_payload))
			throw MalformedPacket{};

		response.auth_file = string_payload.data();
		return;
#else
		break;
#endif

	case TranslationCommand::EXPAND_AUTH_FILE:
#if TRANSLATION_ENABLE_SESSION
		if (response.HasAuth())
			throw DuplicatePacket{};

		if (!IsValidNonEmptyString(string_payload))
			throw MalformedPacket{};

		if (response.regex == nullptr)
			throw MisplacedPacket{};

		response.auth_file = string_payload.data();
		response.expand_auth_file = true;
		return;
#else
		break;
#endif

	case TranslationCommand::APPEND_AUTH:
#if TRANSLATION_ENABLE_SESSION
		if (!response.HasAnyAuth() ||
		    response.append_auth.data() != nullptr ||
		    response.expand_append_auth != nullptr)
			throw MisplacedPacket{};

		response.append_auth = payload;
		return;
#else
		break;
#endif

	case TranslationCommand::EXPAND_APPEND_AUTH:
#if TRANSLATION_ENABLE_SESSION
		if (response.regex == nullptr ||
		    !response.HasAnyAuth() ||
		    response.append_auth.data() != nullptr ||
		    response.expand_append_auth != nullptr)
			throw MisplacedPacket{};

		if (!IsValidNonEmptyString(string_payload))
			throw MalformedPacket{};

		response.expand_append_auth = string_payload.data();
		return;
#else
		break;
#endif

	case TranslationCommand::LISTENER_TAG:
		if (!IsValidNonEmptyString(string_payload))
			throw MalformedPacket{};

		if (response.listener_tag != nullptr)
			throw DuplicatePacket{};

		response.listener_tag = string_payload.data();
		return;

	case TranslationCommand::EXPAND_COOKIE_HOST:
#if TRANSLATION_ENABLE_SESSION
		if (response.regex == nullptr ||
		    resource_address == nullptr ||
		    !resource_address->IsDefined())
			throw MisplacedPacket{};

		if (!IsValidNonEmptyString(string_payload))
			throw MalformedPacket{};

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
			throw MalformedPacket{};

		if (lhttp_address != nullptr) {
			lhttp_address->blocking = false;
		} else
			throw MisplacedPacket{};

		return;
#else
		break;
#endif

	case TranslationCommand::READ_FILE:
		if (response.read_file != nullptr)
			throw DuplicatePacket{};

		if (!IsValidAbsolutePath(string_payload))
			throw MalformedPacket{};

		response.read_file = string_payload.data();
		return;

	case TranslationCommand::EXPAND_READ_FILE:
#if TRANSLATION_ENABLE_EXPAND
		if (response.read_file != nullptr)
			throw DuplicatePacket{};

		if (!IsValidNonEmptyString(string_payload))
			throw MalformedPacket{};

		response.read_file = string_payload.data();
		response.expand_read_file = true;
		return;
#else
		break;
#endif

	case TranslationCommand::EXPAND_HEADER:
#if TRANSLATION_ENABLE_HTTP && TRANSLATION_ENABLE_EXPAND
		if (response.regex == nullptr)
			throw MisplacedPacket{};

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
			throw DuplicatePacket{};

		if (!payload.empty())
			throw MalformedPacket{};

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
			throw MalformedPacket{};

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
			throw DuplicatePacket{};

		if (!payload.empty())
			throw MalformedPacket{};

		response.regex_on_user_uri = true;
		return;
#else
		break;
#endif

	case TranslationCommand::AUTO_GZIP:
#if TRANSLATION_ENABLE_RADDRESS
		if (!payload.empty())
			throw MalformedPacket{};

		if (response.auto_gzip)
			throw MisplacedPacket{};

		response.auto_gzip = true;
		return;
#else
		break;
#endif

	case TranslationCommand::INTERNAL_REDIRECT:
#if TRANSLATION_ENABLE_HTTP
		if (response.internal_redirect.data() != nullptr)
			throw DuplicatePacket{};

		response.internal_redirect = payload;
		return;
#else
		break;
#endif

	case TranslationCommand::HTTP_AUTH:
#if TRANSLATION_ENABLE_HTTP
		if (response.http_auth.data() != nullptr)
			throw DuplicatePacket{};

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
			throw DuplicatePacket{};

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
			throw MalformedPacket{};

		execute_options = &response.service_execute_options.Add(alloc, string_payload.data());
		SetChildOptions(execute_options->child_options);
		return;
#else
		break;
#endif

	case TranslationCommand::INVERSE_REGEX_UNESCAPE:
#if TRANSLATION_ENABLE_EXPAND
		if (!payload.empty())
			throw MalformedPacket{};

		if (response.inverse_regex == nullptr)
			throw MisplacedPacket{};

		if (response.inverse_regex_unescape)
			throw DuplicatePacket{};

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
			throw MalformedPacket{};

		if (response.HasUntrusted())
			throw MisplacedPacket{};

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
			throw MalformedPacket{};

		if (filter == nullptr || filter->reveal_user)
			throw MisplacedPacket{};

		filter->reveal_user = true;
		return;
#else
		break;
#endif

	case TranslationCommand::REALM_FROM_AUTH_BASE:
#if TRANSLATION_ENABLE_SESSION
		if (!payload.empty())
			throw MalformedPacket{};

		if (response.realm_from_auth_base)
			throw DuplicatePacket{};

		if (response.realm != nullptr || !response.HasAuth())
			throw MisplacedPacket{};

		response.realm_from_auth_base = true;
		return;
#else
		break;
#endif

	case TranslationCommand::FORBID_USER_NS:
#if TRANSLATION_ENABLE_SPAWN && defined(HAVE_LIBSECCOMP)
		if (!payload.empty())
			throw MalformedPacket{};

		if (auto &options = MakeChildOptions("misplaced FORBID_USER_NS packet");
		    options.forbid_user_ns)
			throw DuplicatePacket{};
		else
			options.forbid_user_ns = true;
		return;
#else
		break;
#endif

	case TranslationCommand::NO_NEW_PRIVS:
#if TRANSLATION_ENABLE_SPAWN
		if (!payload.empty())
			throw MalformedPacket{};

		if (auto &options = MakeChildOptions("misplaced NO_NEW_PRIVS packet");
		    options.no_new_privs)
			throw DuplicatePacket{};
		else
			options.no_new_privs = true;
		return;
#else
		break;
#endif

	case TranslationCommand::CGROUP:
#if TRANSLATION_ENABLE_SPAWN
		if (!valid_view_name(string_payload.data()))
			throw MalformedPacket{};

		if (auto &options = MakeChildOptions("misplaced CGROUP packet");
		    options.cgroup.name != nullptr)
			throw DuplicatePacket{};
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
			throw MalformedPacket{};

		if (response.external_session_manager != nullptr)
			throw DuplicatePacket{};

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
			throw MalformedPacket{};

		if (response.external_session_manager == nullptr)
			throw MisplacedPacket{};

		if (response.external_session_keepalive != std::chrono::seconds::zero())
			throw DuplicatePacket{};

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
			throw MalformedPacket{};

		if (auto &options = MakeChildOptions("misplaced STDERR_NULL packet");
		    options.stderr_null || options.stderr_path != nullptr)
			throw DuplicatePacket{};
		else
			options.stderr_null = true;
		return;
#else
		break;
#endif

	case TranslationCommand::EXECUTE:
#if TRANSLATION_ENABLE_EXECUTE
		if (!IsValidAbsolutePath(string_payload))
			throw MalformedPacket{};

		if (auto &options = MakeExecuteOptions("misplaced EXECUTE pacxket");
		    options.execute != nullptr)
			throw DuplicatePacket{};
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
			throw MalformedPacket{};

		response.pool = string_payload.data();
		return;

	case TranslationCommand::MESSAGE:
		if (string_payload.size() > 1024 ||
		    !IsValidNonEmptyString(string_payload))
			throw MalformedPacket{};

		response.message = string_payload.data();
		return;

	case TranslationCommand::CANONICAL_HOST:
		if (!IsValidNonEmptyString(string_payload))
			throw MalformedPacket{};

		response.canonical_host = string_payload.data();
		return;

	case TranslationCommand::SHELL:
#if TRANSLATION_ENABLE_EXECUTE
		if (!IsValidAbsolutePath(string_payload))
			throw MalformedPacket{};

		if (auto &options = MakeExecuteOptions("misplaced SHELL packet");
		    options.shell != nullptr)
			throw DuplicatePacket{};
		else
			options.shell = string_payload.data();
		return;
#else
		break;
#endif

	case TranslationCommand::TOKEN:
		if (!IsValidString(string_payload))
			throw MalformedPacket{};
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
			throw MalformedPacket{};

		if (auto &options = MakeNamespaceOptions("misplaced CGROUP_NAMESPACE packet");
		    options.enable_cgroup)
			throw DuplicatePacket{};
		else
			options.enable_cgroup = true;

		return;
#else
		break;
#endif

	case TranslationCommand::REDIRECT_FULL_URI:
#if TRANSLATION_ENABLE_HTTP
		if (!payload.empty())
			throw MalformedPacket{};

		if (response.base == nullptr)
			throw std::runtime_error("REDIRECT_FULL_URI without BASE");

		if (!response.easy_base)
			throw std::runtime_error("REDIRECT_FULL_URI without EASY_BASE");

		if (response.redirect_full_uri)
			throw DuplicatePacket{};

		response.redirect_full_uri = true;
		return;
#else
		break;
#endif

	case TranslationCommand::HTTPS_ONLY:
#if TRANSLATION_ENABLE_HTTP
		if (response.https_only != 0)
			throw DuplicatePacket{};

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
			throw MalformedPacket{};

		return;
#else
		break;
#endif

	case TranslationCommand::FORBID_MULTICAST:
#if TRANSLATION_ENABLE_SPAWN && defined(HAVE_LIBSECCOMP)
		if (!payload.empty())
			throw MalformedPacket{};

		if (auto &options = MakeChildOptions("misplaced FORBID_MULTICAST packet");
		    options.forbid_multicast)
			throw DuplicatePacket{};
		else
			options.forbid_multicast = true;
		return;
#else
		break;
#endif

	case TranslationCommand::FORBID_BIND:
#if TRANSLATION_ENABLE_SPAWN && defined(HAVE_LIBSECCOMP)
		if (!payload.empty())
			throw MalformedPacket{};

		if (auto &options = MakeChildOptions("misplaced FORBID_BIND packet");
		    options.forbid_bind)
			throw DuplicatePacket{};
		else
			options.forbid_bind = true;
		return;
#else
		break;
#endif

	case TranslationCommand::NETWORK_NAMESPACE_NAME:
#if TRANSLATION_ENABLE_SPAWN
		if (!IsValidName(string_payload))
			throw MalformedPacket{};

		if (auto &options = MakeNamespaceOptions("misplaced NETWORK_NAMESPACE_NAME packet");
		    options.network_namespace_name != nullptr)
			throw DuplicatePacket{};
		else if (options.enable_network)
			throw std::runtime_error("Can't combine NETWORK_NAMESPACE_NAME with NETWORK_NAMESPACE");
		else
			options.network_namespace_name = string_payload.data();
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
			throw MalformedPacket{};

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
			throw MisplacedPacket{};

		if (http_address->certificate != nullptr)
			throw DuplicatePacket{};

		if (!IsValidName(string_payload))
			throw MalformedPacket{};

		http_address->certificate = string_payload.data();
		return;
#else
		break;
#endif

	case TranslationCommand::UNCACHED:
#if TRANSLATION_ENABLE_CACHE
#if TRANSLATION_ENABLE_RADDRESS
		if (resource_address == nullptr)
			throw MisplacedPacket{};
#endif

		if (response.uncached)
			throw DuplicatePacket{};

		response.uncached = true;
		return;
#else // !TRANSLATION_ENABLE_CACHE
		break;
#endif

	case TranslationCommand::PID_NAMESPACE_NAME:
#if TRANSLATION_ENABLE_SPAWN
		if (!IsValidName(string_payload))
			throw MalformedPacket{};

		if (auto &options = MakeNamespaceOptions("misplaced PID_NAMESPACE_NAME packet");
		    options.pid.mode != PidNamespaceOptions::Mode::DISABLED)
			throw DuplicatePacket{};
		else {
			options.pid.mode = PidNamespaceOptions::Mode::ACCESSORY;
			options.pid.name = string_payload.data();
		}

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
			throw MalformedPacket{};

#if TRANSLATION_ENABLE_TRANSFORMATION
		if (filter != nullptr) {
			if (filter->cache_tag != nullptr)
				throw DuplicatePacket{};

			filter->cache_tag = string_payload.data();
			return;
		}
#endif // TRANSLATION_ENABLE_TRANSFORMATION

		if (response.address.IsDefined()) {
			if (response.address_cache_tag != nullptr)
				throw DuplicatePacket{};

			response.address_cache_tag = string_payload.data();
			return;
		}

		if (response.cache_tags.Contains(string_payload.data()))
			throw DuplicatePacket{};

		response.cache_tags.Add(alloc, string_payload.data());
#else
		// ignore
#endif // TRANSLATION_ENABLE_CACHE
		return;

	case TranslationCommand::HTTP2:
#if TRANSLATION_ENABLE_HTTP
		if (http_address == nullptr)
			throw MisplacedPacket{};

		if (http_address->http2)
			throw DuplicatePacket{};

		if (!payload.empty())
			throw MalformedPacket{};

		http_address->http2 = true;
		return;
#else
		break;
#endif

	case TranslationCommand::REQUEST_URI_VERBATIM:
#if TRANSLATION_ENABLE_RADDRESS
		if (cgi_address == nullptr)
			throw MisplacedPacket{};

		if (!payload.empty())
			throw MalformedPacket{};

		if (cgi_address->request_uri_verbatim)
			throw DuplicatePacket{};

		cgi_address->request_uri_verbatim = true;
		return;
#else
		break;
#endif

	case TranslationCommand::DEFER:
		if (!payload.empty())
			throw MalformedPacket{};

		response.defer = true;
		return;

	case TranslationCommand::STDERR_POND:
#if TRANSLATION_ENABLE_SPAWN
		if (!payload.empty())
			throw MalformedPacket{};

		if (auto &options = MakeChildOptions("misplaced STDERR_POND packet");
		    options.stderr_pond)
			throw DuplicatePacket{};
		else
			options.stderr_pond = true;
		return;
#else
		break;
#endif

	case TranslationCommand::CHAIN:
#if TRANSLATION_ENABLE_HTTP
		if (response.chain.data() != nullptr)
			throw DuplicatePacket{};

		response.chain = payload;
		return;
#else
		break;
#endif

	case TranslationCommand::BREAK_CHAIN:
#if TRANSLATION_ENABLE_HTTP
		if (!payload.empty())
			throw MalformedPacket{};

		if (!from_request.chain)
			throw std::runtime_error("BREAK_CHAIN without CHAIN request");

		if (response.break_chain)
			throw DuplicatePacket{};

		response.break_chain = true;
		return;
#else
		break;
#endif

	case TranslationCommand::FILTER_NO_BODY:
#if TRANSLATION_ENABLE_TRANSFORMATION
		if (!payload.empty())
			throw MalformedPacket{};

		if (filter == nullptr)
			throw MisplacedPacket{};

		if (filter->no_body)
			throw DuplicatePacket{};

		filter->no_body = true;
		return;
#else
		break;
#endif

	case TranslationCommand::TINY_IMAGE:
#if TRANSLATION_ENABLE_HTTP
		if (!payload.empty())
			throw MalformedPacket{};

		if (response.tiny_image)
			throw DuplicatePacket{};

		response.tiny_image = true;
		return;
#else
		break;
#endif

	case TranslationCommand::ATTACH_SESSION:
#if TRANSLATION_ENABLE_SESSION
		if (payload.empty())
			throw MalformedPacket{};

		if (response.attach_session.data() != nullptr)
			throw DuplicatePacket{};

		response.attach_session = payload;
		return;
#else
		break;
#endif

	case TranslationCommand::LIKE_HOST:
		if (!IsValidNonEmptyString(string_payload))
			throw MalformedPacket{};

		if (response.like_host != nullptr)
			throw DuplicatePacket{};

		response.like_host = string_payload.data();
		return;

	case TranslationCommand::LAYOUT:
#if TRANSLATION_ENABLE_RADDRESS
		if (payload.empty())
			throw MalformedPacket{};

		if (response.layout.data() != nullptr)
			throw DuplicatePacket{};

		response.layout = payload;
		layout_items_builder = std::make_shared<std::vector<TranslationLayoutItem>>();
		return;
#else
		break;
#endif

	case TranslationCommand::RECOVER_SESSION:
#if TRANSLATION_ENABLE_SESSION
		if (response.recover_session != nullptr)
			throw DuplicatePacket{};

		if (string_payload.empty() ||
		    !IsValidCookieValue(string_payload))
			throw MalformedPacket{};

		response.recover_session = string_payload.data();
		return;
#else
		break;
#endif

	case TranslationCommand::OPTIONAL:
		if (!payload.empty())
			throw MalformedPacket{};

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

		throw MisplacedPacket{};

	case TranslationCommand::AUTO_BROTLI_PATH:
#if TRANSLATION_ENABLE_RADDRESS
		if (!payload.empty())
			throw MalformedPacket{};

		if (file_address != nullptr) {
			if (file_address->auto_brotli_path)
				throw DuplicatePacket{};

			file_address->auto_brotli_path = true;
		} else if (from_request.content_type_lookup) {
			if (response.auto_brotli_path)
				throw DuplicatePacket{};

			response.auto_brotli_path = true;
		} else
			throw MisplacedPacket{};
#endif
		return;

	case TranslationCommand::TRANSPARENT_CHAIN:
#if TRANSLATION_ENABLE_HTTP
		if (!payload.empty())
			throw MalformedPacket{};

		if (response.chain.data() == nullptr)
			throw std::runtime_error("TRANSPARENT_CHAIN without CHAIN");

		if (response.transparent_chain)
			throw DuplicatePacket{};

		response.transparent_chain = true;
		return;
#else
		break;
#endif

	case TranslationCommand::STATS_TAG:
		if (!IsValidNonEmptyString(string_payload))
			throw MalformedPacket{};

		if (response.stats_tag != nullptr)
			throw DuplicatePacket{};

		response.stats_tag = string_payload.data();
		return;

	case TranslationCommand::MOUNT_DEV:
#if TRANSLATION_ENABLE_SPAWN
		if (!payload.empty())
			throw MalformedPacket{};

		if (auto &options = MakeMountNamespaceOptions("misplaced MOUNT_DEV packet");
		    (!options.mount_root_tmpfs &&
		     options.pivot_root == nullptr))
			throw MisplacedPacket{};
		else if (options.mount_dev)
			throw DuplicatePacket{};
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
			throw MisplacedPacket{};
#endif

		if (response.eager_cache)
			throw DuplicatePacket{};

		response.eager_cache = true;
		return;
#else // !TRANSLATION_ENABLE_CACHE
		break;
#endif

	case TranslationCommand::AUTO_FLUSH_CACHE:
#if TRANSLATION_ENABLE_CACHE
#if TRANSLATION_ENABLE_RADDRESS
		if (resource_address == nullptr)
			throw MisplacedPacket{};
#endif

		if (response.address_cache_tag == nullptr)
			throw std::runtime_error("AUTO_FLUSH_CACHE without CACHE_TAG");

		if (response.auto_flush_cache)
			throw DuplicatePacket{};

		response.auto_flush_cache = true;
		return;
#else // !TRANSLATION_ENABLE_CACHE
		break;
#endif

	case TranslationCommand::PARALLELISM:
#if TRANSLATION_ENABLE_RADDRESS
		if (payload.size() != 2)
			throw MalformedPacket{};

		if (lhttp_address != nullptr)
			lhttp_address->parallelism = *(const uint16_t *)(const void *)payload.data();
		else if (cgi_address != nullptr)
			cgi_address->parallelism = *(const uint16_t *)(const void *)payload.data();
		else
			throw MisplacedPacket{};
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
			throw DuplicatePacket{};

		if (!IsValidLowerHeaderName(string_payload))
			throw MalformedPacket{};

		response.check_header = string_payload.data();
		return;
#else
		break;
#endif

	case TranslationCommand::CHDIR:
#if TRANSLATION_ENABLE_SPAWN
		if (!IsValidAbsolutePath(string_payload))
			throw MalformedPacket{};

		if (auto &options = MakeChildOptions("misplaced CHDIR packet");
		    options.chdir != nullptr)
			throw DuplicatePacket{};
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
			throw DuplicatePacket{};

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
			throw MalformedPacket{};

		if (response.path_exists)
			throw DuplicatePacket{};

		response.path_exists = true;
		return;
#else
		break;
#endif

	case TranslationCommand::AUTHORIZED_KEYS:
#if TRANSLATION_ENABLE_LOGIN
		if (!IsValidNonEmptyString(string_payload))
			throw MalformedPacket{};

		if (response.authorized_keys != nullptr)
			throw DuplicatePacket{};

		response.authorized_keys = string_payload.data();
		return;
#else
		break;
#endif

	case TranslationCommand::AUTO_BROTLI:
#if TRANSLATION_ENABLE_RADDRESS
		if (!payload.empty())
			throw MalformedPacket{};

		if (response.auto_brotli)
			throw MisplacedPacket{};

		response.auto_brotli = true;
		return;
#else
		break;
#endif

	case TranslationCommand::DISPOSABLE:
#if TRANSLATION_ENABLE_RADDRESS
		if (!payload.empty())
			throw MalformedPacket{};

		if (cgi_address != nullptr &&
		    resource_address->type == ResourceAddress::Type::WAS &&
		    cgi_address->concurrency == 0)
			cgi_address->disposable = true;
		else
			throw MisplacedPacket{};
		return;
#else
		break;
#endif

	case TranslationCommand::DISCARD_QUERY_STRING:
#if TRANSLATION_ENABLE_HTTP
		if (!payload.empty())
			throw MalformedPacket{};

		if (response.discard_query_string)
			throw DuplicatePacket{};

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
			throw MalformedPacket{};

		if (file_address == nullptr)
			throw MisplacedPacket{};

		if (file_address->beneath != nullptr)
			throw DuplicatePacket{};

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
			throw MalformedPacket{};

		if (response.no_home_authorized_keys)
			throw MisplacedPacket{};

		response.no_home_authorized_keys = true;
		return;
#else
		break;
#endif

	case TranslationCommand::TIMEOUT:
		if (payload.size() != 4)
			throw MalformedPacket{};

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
			throw MalformedPacket{};
		response.analytics_id = string_payload.data();
		return;

	case TranslationCommand::GENERATOR:
		if (!IsValidName(string_payload))
			throw MalformedPacket{};
		response.generator = string_payload.data();
		return;

	case TranslationCommand::IGNORE_NO_CACHE:
#if TRANSLATION_ENABLE_CACHE
#if TRANSLATION_ENABLE_RADDRESS
		if (resource_address == nullptr)
			throw MisplacedPacket{};
#endif

		if (response.ignore_no_cache)
			throw DuplicatePacket{};

		response.ignore_no_cache = true;
		return;
#else // !TRANSLATION_ENABLE_CACHE
		break;
#endif

	case TranslationCommand::AUTO_COMPRESS_ONLY_TEXT:
#if TRANSLATION_ENABLE_RADDRESS
		if (!payload.empty())
			throw MalformedPacket{};

		if (response.auto_compress_only_text)
			throw DuplicatePacket{};

		response.auto_compress_only_text = true;
		return;
#else
		break;
#endif

	case TranslationCommand::REGEX_RAW:
#if TRANSLATION_ENABLE_EXPAND
		if (!payload.empty())
			throw MalformedPacket{};

		if (response.regex == nullptr)
			throw MisplacedPacket{};

		if (response.regex_raw)
			throw DuplicatePacket{};

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
			throw MisplacedPacket{};

		HandleTokenBucketParams(response.rate_limit_site_requests, payload);
		return;
#else
		break;
#endif

	case TranslationCommand::ACCEPT_HTTP:
#if TRANSLATION_ENABLE_HTTP
		if (!payload.empty())
			throw MalformedPacket{};

		if (response.accept_http)
			throw DuplicatePacket{};

		response.accept_http = true;
		return;
#else
		break;
#endif

	case TranslationCommand::CAP_SYS_RESOURCE:
#if TRANSLATION_ENABLE_SPAWN && defined(HAVE_LIBCAP)
		if (!payload.empty())
			throw MalformedPacket{};

		if (auto &options = MakeChildOptions("misplaced CAP_SYS_RESOURCE packet");
		    options.cap_sys_resource)
			throw DuplicatePacket{};
		else
			options.cap_sys_resource = true;
		return;
#else
		break;
#endif

	case TranslationCommand::CHROOT:
#if TRANSLATION_ENABLE_SPAWN
		if (!IsValidAbsolutePath(string_payload))
			throw MalformedPacket{};

		if (auto &options = MakeChildOptions("misplaced CHROOT packet");
		    options.chroot != nullptr)
			throw MisplacedPacket{};
		else
			options.chroot = string_payload.data();
		return;
#else
		break;
#endif

	case TranslationCommand::TMPFS_DIRS_READABLE:
#if TRANSLATION_ENABLE_SPAWN
		if (!payload.empty())
			throw MalformedPacket{};

		if (auto &options = MakeMountNamespaceOptions("misplaced TMPFS_DIRS_READABLE packet");
		    !options.IsEnabled())
			throw MisplacedPacket{};
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
			throw MisplacedPacket{};

		HandleTokenBucketParams(response.rate_limit_site_traffic, payload);
		return;
#else
		break;
#endif

	case TranslationCommand::ARCH:
		response.arch = ParseArch(string_payload);
		if (response.arch == Arch::NONE)
			throw MalformedPacket{};

		return;

	case TranslationCommand::ALLOW_PTRACE:
#if TRANSLATION_ENABLE_SPAWN && defined(HAVE_LIBSECCOMP)
		if (!payload.empty())
			throw MalformedPacket{};
		if (auto &options = MakeChildOptions("misplaced ALLOW_PTRACE packet");
		    options.allow_ptrace)
			throw DuplicatePacket{};
		else
			options.allow_ptrace = true;
		return;
#else
		break;
#endif

	case TranslationCommand::ACCESS_CONTROL_ALLOW_ALL:
#if TRANSLATION_ENABLE_RADDRESS
		if (!payload.empty())
			throw MalformedPacket{};

		if (response.layout.data() != nullptr && !layout_items_builder->empty()) {
			auto &item = layout_items_builder->back();

			if (item.access_control_allow_all)
				throw DuplicatePacket{};

			item.access_control_allow_all = true;
			return;
		}

		throw MisplacedPacket{};
#else
		break;
#endif

	case TranslationCommand::MOUNT_TMP_TMPFS_EXEC:
#if TRANSLATION_ENABLE_SPAWN
		HandleMountTmpTmpfsExec(string_payload);
		return;
#else
		break;
#endif

	case TranslationCommand::DIRECTORY_INDEX_SLASH:
#if TRANSLATION_ENABLE_RADDRESS
		if (!payload.empty())
			throw MalformedPacket{};

		if (response.directory_index.data() == nullptr)
			throw MisplacedPacket{};

		if (response.directory_index_slash)
			throw DuplicatePacket{};

		response.directory_index_slash = true;
		return;
#else
		break;
#endif

	case TranslationCommand::APPEND_PATH:
#if TRANSLATION_ENABLE_RADDRESS
		if (!IsValidString(string_payload))
			throw MalformedPacket{};

		if (response.base == nullptr) {
			throw MisplacedPacket{};
		} else if (file_address != nullptr) {
			file_address->append_path = string_payload.data();
			return;
		} else
			throw MisplacedPacket{};
#else
		break;
#endif

	case TranslationCommand::NO_QUERY_STRING:
#if TRANSLATION_ENABLE_HTTP
		if (!payload.empty())
			throw MalformedPacket{};

		if (response.no_query_string)
			throw DuplicatePacket{};

		response.no_query_string = true;
		return;
#else
		break;
#endif

	case TranslationCommand::INSTANT_FADE:
#if TRANSLATION_ENABLE_RADDRESS
		if (!payload.empty())
			throw MalformedPacket{};

		if (cgi_address != nullptr)
			cgi_address->instant_fade = true;
		else if (lhttp_address != nullptr)
			lhttp_address->instant_fade = true;
		else
			throw MisplacedPacket{};

		return;
#else
		break;
#endif

	case TranslationCommand::SIGKILL_:
#if TRANSLATION_ENABLE_SPAWN
		if (!payload.empty())
			throw MalformedPacket{};

		if (auto &options = MakeChildOptions("misplaced SIGKILL packet");
		    options.sigkill)
			throw DuplicatePacket{};
		else
			options.sigkill = true;
		return;
#else
		break;
#endif

	case TranslationCommand::MAX_INOTIFY:
#if TRANSLATION_ENABLE_SPAWN
		HandleMaxInotify(payload);
		return;
#else
		break;
#endif

	case TranslationCommand::PROCESS_NAME:
#if TRANSLATION_ENABLE_SPAWN
		HandleProcessName(string_payload);
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
try {
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
#if TRANSLATION_ENABLE_HTTP
					allow_remote_networks_builder,
#endif
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
} catch (MisplacedPacket) {
	throw FmtRuntimeError("misplaced {:?} packet"sv, ToString(command));
} catch (MalformedPacket) {
	throw FmtRuntimeError("malformed {:?} packet"sv, ToString(command));
} catch (DuplicatePacket) {
	throw FmtRuntimeError("duplicate {:?} packet"sv, ToString(command));
}

TranslateParser::Result
TranslateParser::Process()
{
	if (!reader.IsComplete())
		/* need more data */
		return Result::MORE;

	return HandlePacket(reader.GetCommand(), reader.GetPayload());
}
