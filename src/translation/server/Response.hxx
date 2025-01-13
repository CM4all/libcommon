// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "../Protocol.hxx"
#include "http/Status.hxx"
#include "net/SocketAddress.hxx"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <span>
#include <string_view>
#include <utility>

#include <string.h>

namespace Translation::Server {

class Response {
	std::byte *buffer = nullptr;
	std::size_t capacity = 0, size = 0;

	enum VaryIndex {
		PARAM,
		SESSION,
		LISTENER_TAG,
		LOCAL_ADDRESS,
		REMOTE_HOST,
		HOST,
		LANGUAGE,
		USER_AGENT,
		QUERY_STRING,
		USER,
		INTERNAL_REDIRECT,
		ENOTDIR_,
	};

	static inline constexpr std::array vary_cmds{
		TranslationCommand::PARAM,
		TranslationCommand::SESSION,
		TranslationCommand::LISTENER_TAG,
		TranslationCommand::LOCAL_ADDRESS,
		TranslationCommand::REMOTE_HOST,
		TranslationCommand::HOST,
		TranslationCommand::LANGUAGE,
		TranslationCommand::USER_AGENT,
		TranslationCommand::QUERY_STRING,
		TranslationCommand::USER,
		TranslationCommand::INTERNAL_REDIRECT,
		TranslationCommand::ENOTDIR_,
	};

	std::array<bool, vary_cmds.size()> vary{};

public:
	Response() noexcept
	{
		static constexpr uint8_t protocol_version = 3;
		PacketT(TranslationCommand::BEGIN, protocol_version);
	}

	Response(Response &&other) noexcept
		:buffer(std::exchange(other.buffer, nullptr)),
		 capacity(other.capacity),
		 size(other.size),
		 vary(other.vary) {}

	~Response() noexcept {
		delete[] buffer;
	}

	Response &operator=(Response &&src) noexcept {
		using std::swap;
		swap(buffer, src.buffer);
		swap(capacity, src.capacity);
		swap(size, src.size);
		swap(vary, src.vary);
		return *this;
	}

	/**
	 * An opaque type for Mark() and Revert().
	 */
	struct Marker {
		std::size_t size;
	};

	/**
	 * Returns an opaque marker for later use with Revert().
	 */
	Marker Mark() const noexcept {
		return {size};
	}

	/**
	 * Revert all packets added after the given marker (but leave
	 * "vary" unchanged).
	 */
	void Revert(Marker m) noexcept {
		size = m.size;
	}

	auto &VaryParam() noexcept {
		vary[VaryIndex::PARAM] = true;
		return *this;
	}

	auto &VarySession() noexcept {
		vary[VaryIndex::SESSION] = true;
		return *this;
	}

	auto &VaryListenerTag() noexcept {
		vary[VaryIndex::LISTENER_TAG] = true;
		return *this;
	}

	auto &VaryLocalAddress() noexcept {
		vary[VaryIndex::LOCAL_ADDRESS] = true;
		return *this;
	}

	auto &VaryRemoteHost() noexcept {
		vary[VaryIndex::REMOTE_HOST] = true;
		return *this;
	}

	auto &VaryHost() noexcept {
		vary[VaryIndex::HOST] = true;
		return *this;
	}

	auto &VaryLanguage() noexcept {
		vary[VaryIndex::LANGUAGE] = true;
		return *this;
	}

	auto &VaryUserAgent() noexcept {
		vary[VaryIndex::USER_AGENT] = true;
		return *this;
	}

	auto &VaryQueryString() noexcept {
		vary[VaryIndex::QUERY_STRING] = true;
		return *this;
	}

	auto &VaryUSER() noexcept {
		vary[VaryIndex::USER] = true;
		return *this;
	}

	auto &VaryInternalRedirect() noexcept {
		vary[VaryIndex::INTERNAL_REDIRECT] = true;
		return *this;
	}

	auto &VaryEnotdir() noexcept {
		vary[VaryIndex::ENOTDIR_] = true;
		return *this;
	}

	/**
	 * Append an empty packet.
	 */
	auto &Packet(TranslationCommand cmd) noexcept {
		WriteHeader(cmd, 0);
		return *this;
	}

	Response &Packet(TranslationCommand cmd,
			 std::span<const std::byte> payload) noexcept;

	template<typename T>
	Response &Packet(TranslationCommand cmd,
			 std::span<const T> payload) noexcept {
		return Packet(cmd, std::as_bytes(payload));
	}

	Response &Packet(TranslationCommand cmd, std::nullptr_t) noexcept {
		return Packet(cmd, std::span<const std::byte>{});
	}

	auto &Packet(TranslationCommand cmd,
		     std::string_view payload) noexcept {
		return Packet(cmd, std::span{payload});
	}

	Response &Packet(TranslationCommand cmd,
			 SocketAddress address) noexcept {
		return Packet(cmd, static_cast<std::span<const std::byte>>(address));
	}

	/**
	 * Append a packet by copying the raw bytes of an object.
	 */
	template<typename T>
	Response &PacketT(TranslationCommand cmd, const T &payload) noexcept {
		return Packet(cmd, std::span{&payload, 1});
	}

	/**
	 * Append a packet whose payload is a concatenation of all
	 * parameters.
	 */
	template<typename... Params>
	auto &MultiPacket(TranslationCommand cmd, Params... params) noexcept {
		std::size_t total_length = (... + GetParamLength(params));
		void *p = WriteHeader(cmd, total_length);
		WriteParams(p, params...);
		return *this;
	}

	/**
	 * Append a packet whose payload is a concatenation of all
	 * string parameters.
	 */
	template<typename... Params>
	auto &StringPacket(TranslationCommand cmd, Params... params) noexcept {
		static_assert(sizeof...(params) > 0);
		std::size_t total_length = (... + GetParamLength(params));
		void *p = WriteHeader(cmd, total_length);
		WriteStringParams(p, params...);
		return *this;
	}

	template<typename... Types>
	auto &Token(Types... value) noexcept {
		return StringPacket(TranslationCommand::TOKEN, value...);
	}

	template<typename... Types>
	auto &AnalyticsId(Types... value) noexcept {
		return StringPacket(TranslationCommand::ANALYTICS_ID, value...);
	}

	template<typename... Types>
	auto &Generator(Types... value) noexcept {
		return StringPacket(TranslationCommand::GENERATOR, value...);
	}

	template<typename... Types>
	auto &StatsTag(Types... value) noexcept {
		return StringPacket(TranslationCommand::STATS_TAG, value...);
	}

	template<typename... Types>
	auto &Layout(Types... value) noexcept {
		return StringPacket(TranslationCommand::LAYOUT, value...);
	}

	template<typename... Types>
	auto &Base(Types... value) noexcept {
		return StringPacket(TranslationCommand::BASE, value...);
	}

	auto &UnsafeBase() noexcept {
		return Packet(TranslationCommand::UNSAFE_BASE);
	}

	auto &EasyBase() noexcept {
		return Packet(TranslationCommand::EASY_BASE);
	}

	template<typename... Types>
	auto &Regex(Types... value) noexcept {
		return StringPacket(TranslationCommand::REGEX, value...);
	}

	auto &RegexOnHostUri() noexcept {
		return Packet(TranslationCommand::REGEX_ON_HOST_URI);
	}

	auto &RegexOnUserUri() noexcept {
		return Packet(TranslationCommand::REGEX_ON_USER_URI);
	}

	template<typename... Types>
	auto &InverseRegex(Types... value) noexcept {
		return StringPacket(TranslationCommand::INVERSE_REGEX, value...);
	}

	auto &RegexTail() noexcept {
		return Packet(TranslationCommand::REGEX_TAIL);
	}

	auto &RegexRaw() noexcept {
		return Packet(TranslationCommand::REGEX_RAW);
	}

	auto &RegexUnescape() noexcept {
		return Packet(TranslationCommand::REGEX_UNESCAPE);
	}

	auto &InverseRegexUnescape() noexcept {
		return Packet(TranslationCommand::INVERSE_REGEX_UNESCAPE);
	}

	auto &Defer() noexcept {
		return Packet(TranslationCommand::DEFER);
	}

	auto &Previous() noexcept {
		return Packet(TranslationCommand::PREVIOUS);
	}

	auto &Status(HttpStatus status) noexcept {
		static_assert(sizeof(status) == 2);
		return PacketT(TranslationCommand::STATUS, status);
	}

	auto &AutoGzip() noexcept {
		return Packet(TranslationCommand::AUTO_GZIP);
	}

	auto &AutoBrotli() noexcept {
		return Packet(TranslationCommand::AUTO_BROTLI);
	}

	auto &AutoCompressOnlyText() noexcept {
		return Packet(TranslationCommand::AUTO_COMPRESS_ONLY_TEXT);
	}

	/**
	 * Only for CONTENT_TYPE_LOOKUP responses.
	 */
	auto &AutoGzipped() noexcept {
		return Packet(TranslationCommand::AUTO_GZIPPED);
	}

	/**
	 * Only for CONTENT_TYPE_LOOKUP responses.
	 */
	auto &AutoBrotliPath() noexcept {
		return Packet(TranslationCommand::AUTO_BROTLI_PATH);
	}

	template<typename... Types>
	auto &ListenerTag(Types... value) noexcept {
		return StringPacket(TranslationCommand::LISTENER_TAG, value...);
	}

	template<typename... Types>
	auto &Site(Types... value) noexcept {
		return StringPacket(TranslationCommand::SITE, value...);
	}

	template<typename... Types>
	auto &ExpandSite(Types... value) noexcept {
		return StringPacket(TranslationCommand::EXPAND_SITE, value...);
	}

	template<typename... Types>
	auto &CanonicalHost(Types... value) noexcept {
		return StringPacket(TranslationCommand::CANONICAL_HOST,
				    value...);
	}

	template<typename... Types>
	auto &LikeHost(Types... value) noexcept {
		return StringPacket(TranslationCommand::LIKE_HOST,
				    value...);
	}

	auto &DiscardQueryString() noexcept {
		return Packet(TranslationCommand::DISCARD_QUERY_STRING);
	}

	auto &AllowRemoteNetwork(SocketAddress address,
				 const uint8_t &prefix_length) noexcept {
		return MultiPacket(TranslationCommand::ALLOW_REMOTE_NETWORK,
				   std::span{&prefix_length, 1},
				   static_cast<std::span<const std::byte>>(address));
	}

	auto &RateLimitSiteRequests(const float &rate, const float &burst) noexcept {
		return MultiPacket(TranslationCommand::RATE_LIMIT_SITE_REQUESTS,
				   std::span{&rate, 1},
				   std::span{&burst, 1});
	}

	struct RedirectContext {
		Response &response;

		auto CopyQueryString() noexcept {
			response.Packet(TranslationCommand::REDIRECT_QUERY_STRING);
			return *this;
		}

		auto CopyFullUri() noexcept {
			response.Packet(TranslationCommand::REDIRECT_FULL_URI);
			return *this;
		}

		template<typename... Types>
		auto ExpandRedirect(Types... value) noexcept {
			response.StringPacket(TranslationCommand::EXPAND_REDIRECT, value...);
			return *this;
		}
	};

	template<typename... Types>
	auto Redirect(Types... value) noexcept {
		StringPacket(TranslationCommand::REDIRECT, value...);
		return RedirectContext{*this};
	}

	template<typename... Types>
	auto &Bounce(Types... value) noexcept {
		return StringPacket(TranslationCommand::BOUNCE, value...);
	}

	template<typename... Types>
	auto &Message(Types... value) noexcept {
		return StringPacket(TranslationCommand::MESSAGE, value...);
	}

	auto &TinyImage() noexcept {
		return Packet(TranslationCommand::TINY_IMAGE);
	}

	auto &Scheme(std::string_view value) noexcept {
		return StringPacket(TranslationCommand::SCHEME, value);
	}

	template<typename... Types>
	auto &Host(Types... value) noexcept {
		return StringPacket(TranslationCommand::HOST, value...);
	}

	template<typename... Types>
	auto &Uri(Types... value) noexcept {
		return StringPacket(TranslationCommand::URI, value...);
	}

	template<typename... Types>
	auto &LocalUri(Types... value) noexcept {
		return StringPacket(TranslationCommand::LOCAL_URI, value...);
	}

	template<typename... Types>
	auto &Untrusted(Types... value) noexcept {
		return StringPacket(TranslationCommand::UNTRUSTED, value...);
	}

	template<typename... Types>
	auto &UntrustedPrefix(Types... value) noexcept {
		return StringPacket(TranslationCommand::UNTRUSTED_PREFIX, value...);
	}

	template<typename... Types>
	auto &UntrustedSiteSuffix(Types... value) noexcept {
		return StringPacket(TranslationCommand::UNTRUSTED_SITE_SUFFIX, value...);
	}

	template<typename... Types>
	auto &UntrustedRawSiteSuffix(Types... value) noexcept {
		return StringPacket(TranslationCommand::UNTRUSTED_RAW_SITE_SUFFIX,
				    value...);
	}

	template<typename... Types>
	auto &TestPath(Types... value) noexcept {
		return StringPacket(TranslationCommand::TEST_PATH, value...);
	}

	template<typename... Types>
	auto &ExpandTestPath(Types... value) noexcept {
		return StringPacket(TranslationCommand::EXPAND_TEST_PATH,
				    value...);
	}

	template<typename P>
	auto &FileNotFound(P payload) noexcept {
		return Packet(TranslationCommand::FILE_NOT_FOUND, payload);
	}

	auto &PathExists() noexcept {
		return Packet(TranslationCommand::PATH_EXISTS);
	}

	template<typename... Types>
	auto &Session(Types... value) noexcept {
		return StringPacket(TranslationCommand::SESSION, value...);
	}

	template<typename... Types>
	auto &RealmSession(Types... value) noexcept {
		return StringPacket(TranslationCommand::REALM_SESSION,
				    value...);
	}

	template<typename... Types>
	auto &AttachSession(Types... value) noexcept {
		return StringPacket(TranslationCommand::ATTACH_SESSION,
				    value...);
	}

	auto &Pool(std::string_view name) noexcept {
		return Packet(TranslationCommand::POOL, name);
	}

	template<typename P>
	auto &InternalRedirect(P payload) noexcept {
		return Packet(TranslationCommand::INTERNAL_REDIRECT, payload);
	}

	template<typename... Params>
	auto &HttpAuth(Params... params) noexcept {
		return MultiPacket(TranslationCommand::HTTP_AUTH, params...);
	}

	template<typename... Params>
	auto &TokenAuth(Params... params) noexcept {
		return MultiPacket(TranslationCommand::TOKEN_AUTH, params...);
	}

	template<typename... Types>
	auto &RecoverSession(Types... value) noexcept {
		return StringPacket(TranslationCommand::RECOVER_SESSION,
				    value...);
	}

	template<typename... Types>
	auto Check(Types... value) noexcept {
		return StringPacket(TranslationCommand::CHECK, value...);
	}

	template<typename... Types>
	auto CheckHeader(Types... value) noexcept {
		return StringPacket(TranslationCommand::CHECK_HEADER, value...);
	}

	template<typename... Params>
	auto &Auth(Params... params) noexcept {
		return MultiPacket(TranslationCommand::AUTH, params...);
	}

	template<typename... Types>
	auto &AuthFile(Types... value) noexcept {
		return StringPacket(TranslationCommand::AUTH_FILE,
				    value...);
	}

	template<typename... Types>
	auto &ExpandAuthFile(Types... value) noexcept {
		return StringPacket(TranslationCommand::EXPAND_AUTH_FILE,
				    value...);
	}

	template<typename P>
	auto &AppendAuth(P payload) noexcept {
		return Packet(TranslationCommand::APPEND_AUTH, payload);
	}

	template<typename... Types>
	auto &ExpandAppendAuth(Types... value) noexcept {
		return StringPacket(TranslationCommand::EXPAND_APPEND_AUTH,
				    value...);
	}

	auto &Want(std::initializer_list<TranslationCommand> cmds) noexcept {
		return Packet(TranslationCommand::WANT, std::span{cmds});
	}

	auto &Want(const TranslationCommand &cmd) noexcept {
		return PacketT(TranslationCommand::WANT, cmd);
	}

	template<typename P>
	auto &WantFullUri(P payload) noexcept {
		return Packet(TranslationCommand::WANT_FULL_URI, payload);
	}

	auto &User(std::string_view name) noexcept {
		return Packet(TranslationCommand::USER, name);
	}

	auto &SessionSite(std::string_view value) noexcept {
		return Packet(TranslationCommand::SESSION_SITE, value);
	}

	auto &Language(std::string_view value) noexcept {
		return Packet(TranslationCommand::LANGUAGE, value);
	}

	template<typename... Types>
	auto &Realm(Types... value) noexcept {
		return StringPacket(TranslationCommand::REALM, value...);
	}

	auto &WwwAuthenticate(std::string_view value) noexcept {
		return Packet(TranslationCommand::WWW_AUTHENTICATE, value);
	}

	template<typename... Types>
	auto &Header(std::string_view name, Types... value) noexcept {
		return StringPacket(TranslationCommand::HEADER,
				    name, ":", value...);
	}

	auto &CookieDomain(std::string_view value) noexcept {
		return Packet(TranslationCommand::COOKIE_DOMAIN, value);
	}

	template<typename... Types>
	auto &CookieHost(Types... value) noexcept {
		return StringPacket(TranslationCommand::COOKIE_HOST,
				    value...);
	}

	template<typename... Types>
	auto &ExpandCookieHost(Types... value) noexcept {
		return StringPacket(TranslationCommand::EXPAND_COOKIE_HOST,
				    value...);
	}

	auto &CookiePath(std::string_view value) noexcept {
		return Packet(TranslationCommand::COOKIE_PATH, value);
	}

	/**
	 * Only in reply to TranslationCommand::CONTENT_TYPE_LOOKUP.
	 * For content types of regular files, use
	 * Path().ContentType().
	 */
	template<typename... Types>
	auto &ContentType(Types... value) noexcept {
		return StringPacket(TranslationCommand::CONTENT_TYPE,
				    value...);
	}

	template<typename P>
	auto &ProbePathSuffixes(P payload,
				std::initializer_list<std::string_view> suffixes) noexcept {
		Packet(TranslationCommand::PROBE_PATH_SUFFIXES, payload);
		for (auto i : suffixes)
			StringPacket(TranslationCommand::PROBE_SUFFIX, i);
		return *this;
	}

	template<typename P>
	auto &ProbePathSuffixes(P payload,
				std::span<const std::string_view> suffixes) noexcept {
		Packet(TranslationCommand::PROBE_PATH_SUFFIXES, payload);
		for (auto i : suffixes)
			StringPacket(TranslationCommand::PROBE_SUFFIX, i);
		return *this;
	}

	template<typename... Types>
	auto &ReadFile(Types... path) noexcept {
		return StringPacket(TranslationCommand::READ_FILE, path...);
	}

	template<typename... Types>
	auto &ExpandReadFile(Types... path) noexcept {
		return StringPacket(TranslationCommand::EXPAND_READ_FILE, path...);
	}

	auto &Optional() noexcept {
		return Packet(TranslationCommand::OPTIONAL);
	}

	struct FilterContext {
		Response &response;

		auto Filter4XX() noexcept {
			response.Packet(TranslationCommand::FILTER_4XX);
			return *this;
		}

		auto RevealUser() noexcept {
			response.Packet(TranslationCommand::REVEAL_USER);
			return *this;
		}

		auto NoBody() noexcept {
			response.Packet(TranslationCommand::FILTER_NO_BODY);
			return *this;
		}
	};

	FilterContext Filter() noexcept {
		Packet(TranslationCommand::FILTER);
		return {*this};
	}

	class ProcessorContext {
		Response &response;

	public:
		constexpr explicit
		ProcessorContext(Response &_response) noexcept
			:response(_response) {}

		auto Container() noexcept {
			response.Packet(TranslationCommand::CONTAINER);
			return *this;
		}
	};

	auto Process() noexcept {
		Packet(TranslationCommand::PROCESS);
		return ProcessorContext(*this);
	}

	class MountNamespaceContext {
		Response &response;

	public:
		constexpr explicit
		MountNamespaceContext(Response &_response) noexcept
			:response(_response) {}

		template<typename... Types>
		auto PivotRoot(Types... path) noexcept {
			response.StringPacket(TranslationCommand::PIVOT_ROOT,
					      path...);
			return *this;
		}

		auto MountRootTmpfs() noexcept {
			response.Packet(TranslationCommand::MOUNT_ROOT_TMPFS);
			return *this;
		}

		auto MountProc() noexcept {
			response.Packet(TranslationCommand::MOUNT_PROC);
			return *this;
		}

		auto MountDev() noexcept {
			response.Packet(TranslationCommand::MOUNT_DEV);
			return *this;
		}

		auto MountTmpTmpfs(std::string_view payload={}) noexcept {
			response.StringPacket(TranslationCommand::MOUNT_TMP_TMPFS,
					      payload);
			return *this;
		}

		template<typename... Types>
		auto MountTmpfs(Types... path) noexcept {
			response.StringPacket(TranslationCommand::MOUNT_TMPFS,
					      path...);
			return *this;
		}

		template<typename N, typename T>
		auto MountNamedTmpfs(N &&name, T &&target) noexcept {
			response.StringPacket(TranslationCommand::MOUNT_NAMED_TMPFS,
					      std::forward<N>(name),
					      std::string_view{"", 1},
					      std::forward<T>(target));
			return *this;
		}

		template<typename... Types>
		auto MountEmpty(Types... path) noexcept {
			response.StringPacket(TranslationCommand::MOUNT_EMPTY,
					      path...);
			return *this;
		}

		template<typename... Types>
		auto MountHome(Types... mnt) noexcept {
			response.StringPacket(TranslationCommand::MOUNT_HOME,
					      mnt...);
			return *this;
		}

		template<typename S, typename T>
		auto BindMount(S &&source, T &&target) noexcept {
			response.StringPacket(TranslationCommand::BIND_MOUNT,
					      std::forward<S>(source),
					      std::string_view{"", 1},
					      std::forward<T>(target));
			return *this;
		}

		auto ExpandBindMount(std::string_view source,
				     std::string_view target) noexcept {
			response.StringPacket(TranslationCommand::EXPAND_BIND_MOUNT,
					      source, std::string_view{"", 1},
					      target);
			return *this;
		}

		template<typename S, typename T>
		auto BindMountRw(S &&source, T &&target) noexcept {
			response.StringPacket(TranslationCommand::BIND_MOUNT_RW,
					      std::forward<S>(source),
					      std::string_view{"", 1},
					      std::forward<T>(target));
			return *this;
		}

		auto ExpandBindMountRw(std::string_view source,
				       std::string_view target) noexcept {
			response.StringPacket(TranslationCommand::EXPAND_BIND_MOUNT_RW,
					      source, std::string_view{"", 1},
					      target);
			return *this;
		}

		template<typename S, typename T>
		auto BindMountExec(S &&source, T &&target) noexcept {
			response.StringPacket(TranslationCommand::BIND_MOUNT_EXEC,
					      std::forward<S>(source),
					      std::string_view{"", 1},
					      std::forward<T>(target));
			return *this;
		}

		auto ExpandBindMountExec(std::string_view source,
					 std::string_view target) noexcept {
			response.StringPacket(TranslationCommand::EXPAND_BIND_MOUNT_EXEC,
					      source, std::string_view{"", 1},
					      target);
			return *this;
		}

		template<typename S, typename T>
		auto BindMountFile(S &&source, T &&target) noexcept {
			response.StringPacket(TranslationCommand::BIND_MOUNT_FILE,
					      std::forward<S>(source),
					      std::string_view{"", 1},
					      std::forward<T>(target));
			return *this;
		}

		template<typename... Types>
		auto MountListenStream(Types... mnt) noexcept {
			response.StringPacket(TranslationCommand::MOUNT_LISTEN_STREAM,
					      mnt...);
			return *this;
		}

		template<typename P, typename C>
		auto WriteFile(P &&path, C &&contents) noexcept {
			response.StringPacket(TranslationCommand::WRITE_FILE,
					      std::forward<P>(path),
					      std::string_view{"", 1},
					      std::forward<C>(contents));
			return *this;
		}

		auto Optional() noexcept {
			response.Optional();
			return *this;
		}
	};

	struct CgroupContext {
		Response &response;

		auto Set(std::string_view payload) noexcept {
			response.StringPacket(TranslationCommand::CGROUP_SET,
					      payload);
			return *this;
		}

		template<typename... Types>
		auto Set(std::string_view name, Types... value) noexcept {
			response.StringPacket(TranslationCommand::CGROUP_SET,
					      name, "=", value...);
			return *this;
		}

		auto SetXattr(std::string_view payload) noexcept {
			response.StringPacket(TranslationCommand::CGROUP_XATTR,
					      payload);
			return *this;
		}

		template<typename... Types>
		auto SetXattr(std::string_view name, Types... value) noexcept {
			response.StringPacket(TranslationCommand::CGROUP_XATTR,
					      name, "=", value...);
			return *this;
		}
	};

	class ChildContext {
	protected:
		Response &response;

	public:
		constexpr explicit
		ChildContext(Response &_response) noexcept
			:response(_response) {}

		template<typename... Types>
		auto Tag(Types... value) noexcept {
			static_assert(sizeof...(value) > 0);
			response.StringPacket(TranslationCommand::CHILD_TAG,
					      value...);
			return *this;
		}

		template<typename... Types>
		auto Chroot(Types... path) noexcept {
			response.StringPacket(TranslationCommand::CHROOT,
					      path...);
			return *this;
		}

		template<typename... Types>
		auto Chdir(Types... value) noexcept {
			response.StringPacket(TranslationCommand::CHDIR,
					      value...);
			return *this;
		}

		auto StderrNull() noexcept {
			response.Packet(TranslationCommand::STDERR_NULL);
			return *this;
		}

		auto StderrPond() noexcept {
			response.Packet(TranslationCommand::STDERR_POND);
			return *this;
		}

		template<typename... Types>
		auto StderrPath(Types... value) noexcept {
			static_assert(sizeof...(value) > 0);
			response.StringPacket(TranslationCommand::STDERR_PATH,
					      value...);
			return *this;
		}

		template<typename... Types>
		auto ExpandStderrPath(Types... value) noexcept {
			static_assert(sizeof...(value) > 0);
			response.StringPacket(TranslationCommand::EXPAND_STDERR_PATH,
					      value...);
			return *this;
		}

		template<typename... Types>
		auto StderrPathJailed(Types... value) noexcept {
			static_assert(sizeof...(value) > 0);
			response.StringPacket(TranslationCommand::STDERR_PATH_JAILED,
					      value...);
			return *this;
		}

		auto SetEnv(std::string_view s) noexcept {
			response.StringPacket(TranslationCommand::SETENV, s);
			return *this;
		}

		template<typename... Types>
		auto SetEnv(std::string_view name,
			    Types... value) noexcept {
			response.StringPacket(TranslationCommand::SETENV,
					      name, "=", value...);
			return *this;
		}

		auto ExpandSetEnv(std::string_view s) noexcept {
			response.StringPacket(TranslationCommand::EXPAND_SETENV,
					      s);
			return *this;
		}

		template<typename... Types>
		auto ExpandSetEnv(std::string_view name,
				  Types... value) noexcept {
			response.StringPacket(TranslationCommand::EXPAND_SETENV,
					      name, "=", value...);
			return *this;
		}

		template<typename... Types>
		auto Append(Types... value) noexcept {
			response.StringPacket(TranslationCommand::APPEND,
					      value...);
			return *this;
		}

		template<typename... Types>
		auto ExpandAppend(Types... value) noexcept {
			response.StringPacket(TranslationCommand::EXPAND_APPEND,
					      value...);
			return *this;
		}

		template<typename... Types>
		CgroupContext Cgroup(Types... value) noexcept {
			response.StringPacket(TranslationCommand::CGROUP,
					      value...);
			return {response};
		}

		template<typename... Types>
		auto Home(Types... value) noexcept {
			response.StringPacket(TranslationCommand::HOME,
					      value...);
			return *this;
		}

		template<typename... Types>
		auto ExpandHome(Types... value) noexcept {
			response.StringPacket(TranslationCommand::EXPAND_HOME,
					      value...);
			return *this;
		}

		template<typename... Types>
		auto Shell(Types... value) noexcept {
			response.StringPacket(TranslationCommand::SHELL,
					      value...);
			return *this;
		}

		template<typename... Types>
		auto Rlimits(Types... value) noexcept {
			response.StringPacket(TranslationCommand::RLIMITS,
					      value...);
			return *this;
		}

		auto UserNamespace() noexcept {
			response.Packet(TranslationCommand::USER_NAMESPACE);
			return *this;
		}

		auto PidNamespace() noexcept {
			response.Packet(TranslationCommand::PID_NAMESPACE);
			return *this;
		}

		template<typename... Types>
		auto PidNamespace(Types... name) noexcept {
			response.Packet(TranslationCommand::PID_NAMESPACE_NAME,
					name...);
			return *this;
		}

		auto CgroupNamespace() noexcept {
			response.Packet(TranslationCommand::CGROUP_NAMESPACE);
			return *this;
		}

		auto NetworkNamespace() noexcept {
			response.Packet(TranslationCommand::NETWORK_NAMESPACE);
			return *this;
		}

		auto NetworkNamespace(std::string_view name) noexcept {
			response.StringPacket(TranslationCommand::NETWORK_NAMESPACE_NAME,
					      name);
			return *this;
		}

		auto IpcNamespace() noexcept {
			response.Packet(TranslationCommand::IPC_NAMESPACE);
			return *this;
		}

		auto UtsNamespace(std::string_view hostname) noexcept {
			response.StringPacket(TranslationCommand::UTS_NAMESPACE,
					      hostname);
			return *this;
		}

		MountNamespaceContext MountNamespace() noexcept {
			return MountNamespaceContext(response);
		}

		auto UidGid(const uint32_t &uid, const uint32_t &gid,
			    std::span<const uint32_t> supplementary_groups={}) noexcept {
			response.MultiPacket(TranslationCommand::UID_GID,
					     std::span{&uid, 1},
					     std::span{&gid, 1},
					     supplementary_groups);
			return *this;
		}

		auto MappedUid(const uint32_t &uid) noexcept {
			response.PacketT(TranslationCommand::MAPPED_UID_GID, uid);
			return *this;
		}

		auto Umask(uint16_t mask) noexcept {
			response.PacketT(TranslationCommand::UMASK, mask);
			return *this;
		}

		auto ForbidUserNamespace() noexcept {
			response.Packet(TranslationCommand::FORBID_USER_NS);
			return *this;
		}

		auto ForbidMulticast() noexcept {
			response.Packet(TranslationCommand::FORBID_MULTICAST);
			return *this;
		}

		auto ForbidBind() noexcept {
			response.Packet(TranslationCommand::FORBID_BIND);
			return *this;
		}

		auto CapSysResource() noexcept {
			response.Packet(TranslationCommand::CAP_SYS_RESOURCE);
			return *this;
		}

		auto NoNewPrivs() noexcept {
			response.Packet(TranslationCommand::NO_NEW_PRIVS);
			return *this;
		}
	};

	template<typename... Types>
	auto Execute(Types... path) noexcept {
		StringPacket(TranslationCommand::EXECUTE, path...);
		return ChildContext(*this);
	}

	template<typename... Types>
	auto Pipe(Types... path) noexcept {
		StringPacket(TranslationCommand::PIPE, path...);
		return ChildContext(*this);
	}

	class CgiAlikeChildContext : public ChildContext {
	public:
		using ChildContext::ChildContext;

		auto AutoBase() noexcept {
			response.Packet(TranslationCommand::AUTO_BASE);
			return *this;
		}

		auto RequestUriVerbatim() noexcept {
			response.Packet(TranslationCommand::REQUEST_URI_VERBATIM);
			return *this;
		}

		template<typename... Types>
		auto ExpandPath(Types... value) noexcept {
			response.StringPacket(TranslationCommand::EXPAND_PATH,
					      value...);
			return *this;
		}

		template<typename... Types>
		auto Action(Types... value) noexcept {
			response.StringPacket(TranslationCommand::ACTION,
					      value...);
			return *this;
		}

		template<typename... Types>
		auto Uri(Types... value) noexcept {
			response.StringPacket(TranslationCommand::URI,
					      value...);
			return *this;
		}

		template<typename... Types>
		auto ExpandUri(Types... value) noexcept {
			response.StringPacket(TranslationCommand::EXPAND_URI,
					      value...);
			return *this;
		}

		template<typename... Types>
		auto ScriptName(Types... value) noexcept {
			response.StringPacket(TranslationCommand::SCRIPT_NAME,
					      value...);
			return *this;
		}

		template<typename... Types>
		auto ExpandScriptName(Types... value) noexcept {
			response.StringPacket(TranslationCommand::EXPAND_SCRIPT_NAME,
					      value...);
			return *this;
		}

		template<typename... Types>
		auto PathInfo(Types... value) noexcept {
			response.StringPacket(TranslationCommand::PATH_INFO,
					      value...);
			return *this;
		}

		template<typename... Types>
		auto ExpandPathInfo(Types... value) noexcept {
			response.StringPacket(TranslationCommand::EXPAND_PATH_INFO,
					      value...);
			return *this;
		}

		template<typename... Types>
		auto QueryString(Types... value) noexcept {
			response.StringPacket(TranslationCommand::QUERY_STRING,
					      value...);
			return *this;
		}
	};

	class WasChildContext : public CgiAlikeChildContext {
	public:
		using CgiAlikeChildContext::CgiAlikeChildContext;

		auto Address(SocketAddress address) noexcept {
			response.Packet(TranslationCommand::ADDRESS, address);
			return *this;
		}

		auto Parameter(std::string_view s) noexcept {
			response.StringPacket(TranslationCommand::PAIR, s);
			return *this;
		}

		template<typename... Types>
		auto Parameter(std::string_view name,
			       Types... value) noexcept {
			response.StringPacket(TranslationCommand::PAIR,
					      name, "=", value...);
			return *this;
		}

		auto ExpandParameter(std::string_view s) noexcept {
			response.StringPacket(TranslationCommand::EXPAND_PAIR,
					      s);
			return *this;
		}

		template<typename... Types>
		auto ExpandParameter(std::string_view name,
				     Types... value) noexcept {
			response.MultiPacket(TranslationCommand::EXPAND_PAIR,
					     name, "=", value...);
			return *this;
		}

		auto Parallelism(uint16_t value) noexcept {
			response.PacketT(TranslationCommand::PARALLELISM,
					 value);
			return *this;
		}

		auto Concurrency(uint16_t value) noexcept {
			response.PacketT(TranslationCommand::CONCURRENCY,
					 value);
			return *this;
		}

		auto Disposable() noexcept {
			response.Packet(TranslationCommand::DISPOSABLE);
			return *this;
		}
	};

	template<typename... Types>
	WasChildContext Was(Types... path) noexcept {
		StringPacket(TranslationCommand::WAS, path...);
		return WasChildContext(*this);
	}

	class FastCgiChildContext : public CgiAlikeChildContext {
	public:
		using CgiAlikeChildContext::CgiAlikeChildContext;

		auto Parameter(std::string_view s) noexcept {
			response.StringPacket(TranslationCommand::PAIR, s);
			return *this;
		}

		template<typename... Types>
		auto Parameter(std::string_view name,
			       Types... value) noexcept {
			response.MultiPacket(TranslationCommand::PAIR,
					     name, "=", value...);
			return *this;
		}

		auto ExpandParameter(std::string_view s) noexcept {
			response.StringPacket(TranslationCommand::EXPAND_PAIR,
					      s);
			return *this;
		}

		template<typename... Types>
		auto DocumentRoot(Types... value) noexcept {
			response.StringPacket(TranslationCommand::DOCUMENT_ROOT,
					      value...);
			return *this;
		}

		template<typename... Types>
		auto ExpandDocumentRoot(Types... value) noexcept {
			response.StringPacket(TranslationCommand::EXPAND_DOCUMENT_ROOT,
					      value...);
			return *this;
		}

		auto Parallelism(uint16_t value) noexcept {
			response.PacketT(TranslationCommand::PARALLELISM,
					 value);
			return *this;
		}

		auto Concurrency(uint16_t value) noexcept {
			response.PacketT(TranslationCommand::CONCURRENCY,
					 value);
			return *this;
		}
	};

	template<typename... Types>
	FastCgiChildContext FastCGI(Types... path) noexcept {
		StringPacket(TranslationCommand::FASTCGI, path...);
		return FastCgiChildContext(*this);
	}

	class CgiChildContext : public CgiAlikeChildContext {
	public:
		using CgiAlikeChildContext::CgiAlikeChildContext;

		template<typename... Types>
		auto Interpreter(Types... value) noexcept {
			response.StringPacket(TranslationCommand::INTERPRETER,
					      value...);
			return *this;
		}

		template<typename... Types>
		auto DocumentRoot(Types... value) noexcept {
			response.StringPacket(TranslationCommand::DOCUMENT_ROOT,
					      value...);
			return *this;
		}

		template<typename... Types>
		auto ExpandDocumentRoot(Types... value) noexcept {
			response.StringPacket(TranslationCommand::EXPAND_DOCUMENT_ROOT,
					      value...);
			return *this;
		}
	};

	template<typename... Types>
	CgiChildContext CGI(Types... path) noexcept {
		StringPacket(TranslationCommand::CGI, path...);
		return CgiChildContext(*this);
	}

	class LhttpChildContext : public ChildContext {
	public:
		using ChildContext::ChildContext;

		template<typename... Types>
		auto Uri(Types... uri) noexcept {
			response.StringPacket(TranslationCommand::LHTTP_URI,
					      uri...);
			return *this;
		}

		template<typename... Types>
		auto ExpandUri(Types... uri) noexcept {
			response.StringPacket(TranslationCommand::EXPAND_LHTTP_URI,
					      uri...);
			return *this;
		}

		template<typename... Types>
		auto Host(Types... name) noexcept {
			response.StringPacket(TranslationCommand::LHTTP_HOST,
					      name...);
			return *this;
		}

		template<typename... Types>
		auto RequestHeader(std::string_view name,
				   Types... value) noexcept {
			response.StringPacket(TranslationCommand::REQUEST_HEADER,
					      name, ":", value...);
			return *this;
		}

		template<typename... Types>
		auto ExpandRequestHeader(std::string_view name,
				   Types... value) noexcept {
			response.StringPacket(TranslationCommand::EXPAND_REQUEST_HEADER,
					      name, ":", value...);
			return *this;
		}

		auto NonBlocking() noexcept {
			response.Packet(TranslationCommand::NON_BLOCKING);
			return *this;
		}

		auto Parallelism(uint16_t value) noexcept {
			response.PacketT(TranslationCommand::PARALLELISM,
					 value);
			return *this;
		}

		auto Concurrency(uint16_t value) noexcept {
			response.PacketT(TranslationCommand::CONCURRENCY,
					 value);
			return *this;
		}
	};

	auto Lhttp(std::string_view path) noexcept {
		StringPacket(TranslationCommand::LHTTP_PATH, path);
		return LhttpChildContext(*this);
	}

	class FileContext {
		Response &response;

	public:
		constexpr explicit
		FileContext(Response &_response) noexcept
			:response(_response) {}

		template<typename... Types>
		auto ExpandPath(Types... value) noexcept {
			response.StringPacket(TranslationCommand::EXPAND_PATH,
					      value...);
			return *this;
		}

		template<typename... Types>
		auto ContentType(Types... value) noexcept {
			response.StringPacket(TranslationCommand::CONTENT_TYPE,
					      value...);
			return *this;
		}

		template<typename P>
		auto ContentTypeLookup(P payload) noexcept {
			response.StringPacket(TranslationCommand::CONTENT_TYPE_LOOKUP,
					      payload);
			return *this;
		}

		template<typename... Types>
		auto Gzipped(std::string_view path) noexcept {
			response.StringPacket(TranslationCommand::GZIPPED, path);
			return *this;
		}

		template<typename... Types>
		auto DocumentRoot(Types... value) noexcept {
			response.StringPacket(TranslationCommand::DOCUMENT_ROOT,
					      value...);
			return *this;
		}

		template<typename... Types>
		auto ExpandDocumentRoot(Types... value) noexcept {
			response.StringPacket(TranslationCommand::EXPAND_DOCUMENT_ROOT,
					      value...);
			return *this;
		}

		ChildContext Delegate(std::string_view helper) noexcept {
			response.StringPacket(TranslationCommand::DELEGATE, helper);
			return ChildContext(response);
		}

		template<typename P>
		auto DirectoryIndex(P payload) noexcept {
			response.Packet(TranslationCommand::DIRECTORY_INDEX,
					payload);
			return *this;
		}

		template<typename P>
		auto Enotdir(P payload) noexcept {
			response.Packet(TranslationCommand::ENOTDIR_,
					payload);
			return *this;
		}

		auto AutoGzipped() noexcept {
			response.Packet(TranslationCommand::AUTO_GZIPPED);
			return *this;
		}

		auto AutoBrotliPath() noexcept {
			response.Packet(TranslationCommand::AUTO_BROTLI_PATH);
			return *this;
		}

		template<typename... Types>
		auto Beneath(Types... value) noexcept {
			response.StringPacket(TranslationCommand::BENEATH,
					      value...);
			return *this;
		}
	};

	template<typename... Types>
	auto Path(Types... path) noexcept {
		StringPacket(TranslationCommand::PATH, path...);
		return FileContext(*this);
	}

	class HttpContext {
		Response &response;

	public:
		constexpr explicit
		HttpContext(Response &_response) noexcept
			:response(_response) {}

		template<typename... Types>
		auto ExpandPath(Types... value) noexcept {
			response.StringPacket(TranslationCommand::EXPAND_PATH,
					      value...);
			return *this;
		}

		auto Address(SocketAddress address) noexcept {
			response.Packet(TranslationCommand::ADDRESS, address);
			return *this;
		}

		template<typename A>
		auto Addresses(const A &addresses) noexcept {
			for (const auto &i : addresses)
				Address(i);
			return *this;
		}

		auto Http2() noexcept {
			response.Packet(TranslationCommand::HTTP2);
			return *this;
		}

		auto Sticky() noexcept {
			response.Packet(TranslationCommand::STICKY);
			return *this;
		}

		template<typename... Types>
		auto RequestHeader(std::string_view name,
				   Types... value) noexcept {
			response.StringPacket(TranslationCommand::REQUEST_HEADER,
					      name, ":", value...);
			return *this;
		}

		template<typename... Types>
		auto ExpandRequestHeader(std::string_view name,
				   Types... value) noexcept {
			response.StringPacket(TranslationCommand::EXPAND_REQUEST_HEADER,
					      name, ":", value...);
			return *this;
		}
	};

	template<typename... Types>
	HttpContext Http(Types... url) noexcept {
		StringPacket(TranslationCommand::HTTP, url...);
		return HttpContext(*this);
	}

	auto &View(std::string_view name) noexcept {
		return Packet(TranslationCommand::VIEW, name);
	}

	auto &HttpsOnly() noexcept {
		return Packet(TranslationCommand::HTTPS_ONLY);
	}

	auto &HttpsOnly(uint16_t port) noexcept {
		return PacketT(TranslationCommand::HTTPS_ONLY, port);
	}

	auto &Uncached() noexcept {
		return Packet(TranslationCommand::UNCACHED);
	}

	auto &IgnoreNoCache() noexcept {
		return Packet(TranslationCommand::IGNORE_NO_CACHE);
	}

	auto &EagerCache() noexcept {
		return Packet(TranslationCommand::EAGER_CACHE);
	}

	auto &AutoFlushCache() noexcept {
		return Packet(TranslationCommand::AUTO_FLUSH_CACHE);
	}

	template<typename... Types>
	auto &CacheTag(Types... tag) noexcept {
		return StringPacket(TranslationCommand::CACHE_TAG,
				    tag...);
	}

	auto &Stateful() noexcept {
		return Packet(TranslationCommand::STATEFUL);
	}

	auto &DiscardSession() noexcept {
		return Packet(TranslationCommand::DISCARD_SESSION);
	}

	auto &DiscardRealmSession() noexcept {
		return Packet(TranslationCommand::DISCARD_REALM_SESSION);
	}

	auto &SecureCookie() noexcept {
		return Packet(TranslationCommand::SECURE_COOKIE);
	}

	auto &SessionCookieSameSite(std::string_view same_site) noexcept {
		return StringPacket(TranslationCommand::SESSION_COOKIE_SAME_SITE,
				    same_site);
	}

	auto &RequireCsrfToken() noexcept {
		return Packet(TranslationCommand::REQUIRE_CSRF_TOKEN);
	}

	auto &SendCsrfToken() noexcept {
		return Packet(TranslationCommand::SEND_CSRF_TOKEN);
	}

	auto &RealmFromAuthBase() noexcept {
		return Packet(TranslationCommand::REALM_FROM_AUTH_BASE);
	}

	auto &Transparent() noexcept {
		return Packet(TranslationCommand::TRANSPARENT);
	}

	auto &WidgetInfo() noexcept {
		return Packet(TranslationCommand::WIDGET_INFO);
	}

	auto &AnchorAbsolute() noexcept {
		return Packet(TranslationCommand::ANCHOR_ABSOLUTE);
	}

	auto &ExpiresRelative(const std::chrono::duration<uint32_t> &seconds) noexcept {
		return PacketT(TranslationCommand::EXPIRES_RELATIVE, seconds);
	}

	auto &ExpiresRelativeWithQuery(const std::chrono::duration<uint32_t> &seconds) noexcept {
		return PacketT(TranslationCommand::EXPIRES_RELATIVE_WITH_QUERY, seconds);
	}

	template<typename... Types>
	auto &Chain(Types... value) noexcept {
		return StringPacket(TranslationCommand::CHAIN, value...);
	}

	auto &TransparentChain() noexcept {
		return Packet(TranslationCommand::TRANSPARENT_CHAIN);
	}

	auto &BreakChain() noexcept {
		return Packet(TranslationCommand::BREAK_CHAIN);
	}

	template<typename... Types>
	auto &NoPassword(Types... payload) noexcept {
		return StringPacket(TranslationCommand::NO_PASSWORD,
				    payload...);
	}

	template<typename... Types>
	auto &AuthorizedKeys(Types... payload) noexcept {
		return StringPacket(TranslationCommand::AUTHORIZED_KEYS,
				    payload...);
	}

	auto &NoHomeAuthorizedKeys() noexcept {
		return Packet(TranslationCommand::NO_HOME_AUTHORIZED_KEYS);
	}

	auto &MaxAge(uint32_t seconds) noexcept {
		return PacketT(TranslationCommand::MAX_AGE, seconds);
	}

	auto &Timeout(const std::chrono::duration<uint32_t> seconds) noexcept {
		return PacketT(TranslationCommand::TIMEOUT, seconds);
	}

	auto &AcceptHttp() noexcept {
		return Packet(TranslationCommand::ACCEPT_HTTP);
	}

	std::span<std::byte> Finish() noexcept;

private:
	void Grow(std::size_t new_capacity) noexcept;
	void *Write(std::size_t nbytes) noexcept;

	void *WriteHeader(TranslationCommand cmd,
			  std::size_t payload_size) noexcept;

	static constexpr std::size_t GetParamLength(std::span<const std::byte> src) noexcept {
		return src.size();
	}

	template<typename T>
#ifndef __clang__
	/* disabling constexpr on clang to work around bogus
	   -Wundefined-inline */
	constexpr
#endif
	static std::size_t GetParamLength(std::span<const T> src) noexcept {
		return GetParamLength(std::as_bytes(src));
	}

	static constexpr std::size_t GetParamLength(std::string_view src) noexcept {
		return GetParamLength(std::span{src});
	}

	template<typename T>
	static constexpr std::size_t GetParamLength(std::initializer_list<T> l) noexcept {
		return std::accumulate(l.begin(), l.end(), size_t(0),
				       [](std::size_t a, const T &b){
					       return a + GetParamLength(b);
				       });
	}

	static void *WriteParam(void *dest, std::span<const std::byte> src) noexcept {
		return mempcpy(dest, src.data(), src.size());
	}

	template<typename T>
	static void *WriteParam(void *dest, std::span<const T> src) noexcept {
		return WriteParam(dest, std::as_bytes(src));
	}

	static void *WriteParam(void *dest, std::string_view src) noexcept {
		return WriteParam(dest, std::span{src});
	}

	template<typename T>
	static void *WriteParam(void *dest, std::initializer_list<T> l) noexcept {
		for (auto &i : l)
			dest = WriteParam(i);
		return dest;
	}

	template<typename P, typename... Params>
	static void *WriteParams(void *dest, P first, Params... value) noexcept {
		dest = WriteParam(dest, first);
		return WriteParams(dest, value...);
	}

	static void *WriteParams(void *dest) noexcept {
		return dest;
	}

	static void *WriteStringParam(void *dest, std::string_view src) noexcept {
		return mempcpy(dest, src.data(), src.size());
	}

	static void *WriteStringParam(void *dest, std::initializer_list<std::string_view> l) noexcept {
		for (auto &i : l)
			dest = WriteStringParam(dest, i);
		return dest;
	}

	template<typename P, typename... Params>
	static void *WriteStringParams(void *dest, P first,
				       Params... value) noexcept {
		dest = WriteStringParam(dest, first);
		return WriteStringParams(dest, value...);
	}

	static void *WriteStringParams(void *dest) noexcept {
		return dest;
	}
};

} // namespace Translation::Server
