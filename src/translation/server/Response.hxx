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

#pragma once

#include "../Protocol.hxx"
#include "http/Status.h"
#include "net/SocketAddress.hxx"
#include "util/ConstBuffer.hxx"
#include "util/WritableBuffer.hxx"

#include <array>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <string_view>
#include <utility>

#include <string.h>

namespace Translation::Server {

class Response {
	uint8_t *buffer = nullptr;
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
		Packet(TranslationCommand::BEGIN,
		       {&protocol_version, sizeof(protocol_version)});
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
			 ConstBuffer<void> payload) noexcept;

	Response &Packet(TranslationCommand cmd, std::nullptr_t n) noexcept {
		return Packet(cmd, ConstBuffer<void>{n});
	}

	auto &Packet(TranslationCommand cmd,
		     std::string_view payload) noexcept {
		return Packet(cmd, ConstBuffer<void>{payload.data(), payload.size()});
	}

	/**
	 * Append a packet by copying the raw bytes of an object.
	 */
	template<typename T>
	auto &PacketT(TranslationCommand cmd, const T &payload) noexcept {
		return Packet(cmd, ConstBuffer<void>(&payload, sizeof(payload)));
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

	auto &Status(http_status_t _status) noexcept {
		const uint16_t status = uint16_t(_status);
		return PacketT(TranslationCommand::STATUS, status);
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

	template<typename P>
	auto &Session(P payload) noexcept {
		return Packet(TranslationCommand::SESSION, payload);
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

	template<typename P>
	auto &RecoverSession(P payload) noexcept {
		return Packet(TranslationCommand::RECOVER_SESSION, payload);
	}

	template<typename P>
	auto &Check(P payload) noexcept {
		return Packet(TranslationCommand::CHECK, payload);
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
		ConstBuffer<TranslationCommand> payload(&*cmds.begin(),
							cmds.size());
		return Packet(TranslationCommand::WANT, payload.ToVoid());
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
				ConstBuffer<std::string_view> suffixes) noexcept {
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

		template<typename... Types>
		auto CacheTag(Types... tag) noexcept {
			response.StringPacket(TranslationCommand::CACHE_TAG,
					      tag...);
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
	};

	class ChildContext {
	protected:
		Response &response;

	public:
		constexpr explicit
		ChildContext(Response &_response) noexcept
			:response(_response) {}

		auto Tag(std::string_view value) noexcept {
			response.StringPacket(TranslationCommand::CHILD_TAG, value);
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

		auto UidGid(uint32_t uid, uint32_t gid,
			    ConstBuffer<uint32_t> supplementary_groups=nullptr) noexcept {
			response.MultiPacket(TranslationCommand::UID_GID,
					     ConstBuffer<void>(&uid, sizeof(uid)),
					     ConstBuffer<void>(&gid, sizeof(gid)),
					     supplementary_groups.ToVoid());
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

		auto NoNewPrivs() noexcept {
			response.Packet(TranslationCommand::NO_NEW_PRIVS);
			return *this;
		}
	};

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
	};

	template<typename... Types>
	FastCgiChildContext FastCGI(Types... path) noexcept {
		Packet(TranslationCommand::FASTCGI, path...);
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

		auto NonBlocking() noexcept {
			response.Packet(TranslationCommand::NON_BLOCKING);
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
		auto Deflated(std::string_view path) noexcept {
			response.StringPacket(TranslationCommand::DEFLATED, path);
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
			response.Packet(TranslationCommand::ADDRESS,
					ConstBuffer<void>{address.GetAddress(), address.GetSize()});
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

	auto &ExpiresRelative(uint32_t seconds) noexcept {
		return PacketT(TranslationCommand::EXPIRES_RELATIVE, seconds);
	}

	template<typename P>
	auto &Chain(P payload) noexcept {
		return Packet(TranslationCommand::CHAIN, payload);
	}

	auto &BreakChain() noexcept {
		return Packet(TranslationCommand::BREAK_CHAIN);
	}

	auto &MaxAge(uint32_t seconds) noexcept {
		return PacketT(TranslationCommand::MAX_AGE, seconds);
	}

	WritableBuffer<uint8_t> Finish() noexcept;

private:
	void Grow(std::size_t new_capacity) noexcept;
	void *Write(std::size_t nbytes) noexcept;

	void *WriteHeader(TranslationCommand cmd,
			  std::size_t payload_size) noexcept;

	static constexpr std::size_t GetParamLength(ConstBuffer<void> b) noexcept {
		return b.size;
	}

	static constexpr std::size_t GetParamLength(std::string_view sv) noexcept {
		return sv.size();
	}

	template<typename T>
	static constexpr std::size_t GetParamLength(std::initializer_list<T> l) noexcept {
		return std::accumulate(l.begin(), l.end(), size_t(0),
				       [](std::size_t a, const T &b){
					       return a + GetParamLength(b);
				       });
	}

	static void *WriteParam(void *dest, ConstBuffer<void> src) noexcept {
		return mempcpy(dest, src.data, src.size);
	}

	static void *WriteParam(void *dest, std::string_view src) noexcept {
		return mempcpy(dest, src.data(), src.size());
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
