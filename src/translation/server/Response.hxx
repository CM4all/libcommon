/*
 * Copyright 2007-2020 CM4all GmbH
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
#include <string_view>
#include <utility>

#include <stdint.h>
#include <stddef.h>
#include <string.h>

namespace Translation::Server {

class Response {
	uint8_t *buffer = nullptr;
	size_t capacity = 0, size = 0;

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
		size_t total_length = (... + GetParamLength(params));
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
		size_t total_length = (... + GetParamLength(params));
		void *p = WriteHeader(cmd, total_length);
		WriteStringParams(p, params...);
		return *this;
	}

	template<typename... Types>
	auto &Token(Types... value) noexcept {
		return StringPacket(TranslationCommand::TOKEN, value...);
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

	auto &Status(http_status_t _status) noexcept {
		const uint16_t status = uint16_t(_status);
		return PacketT(TranslationCommand::STATUS, status);
	}

	template<typename... Types>
	auto &Site(Types... value) noexcept {
		return StringPacket(TranslationCommand::SITE, value...);
	}

	template<typename... Types>
	auto &CanonicalHost(Types... value) noexcept {
		return StringPacket(TranslationCommand::CANONICAL_HOST,
				    value...);
	}

	struct RedirectContext {
		Response &response;

		auto CopyQueryString() noexcept {
			response.Packet(TranslationCommand::REDIRECT_QUERY_STRING);
			return *this;
		}
	};

	template<typename... Types>
	auto Redirect(Types... value) noexcept {
		StringPacket(TranslationCommand::REDIRECT, value...);
		return RedirectContext{*this};
	}

	template<typename... Types>
	auto ExpandRedirect(Types... value) noexcept {
		StringPacket(TranslationCommand::EXPAND_REDIRECT, value...);
		return RedirectContext{*this};
	}

	template<typename... Types>
	auto &Bounce(Types... value) noexcept {
		return StringPacket(TranslationCommand::BOUNCE, value...);
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
	auto &ProbePathSuffixes(P payload,
				std::initializer_list<std::string_view> suffixes) noexcept {
		Packet(TranslationCommand::PROBE_PATH_SUFFIXES, payload);
		for (auto i : suffixes)
			StringPacket(TranslationCommand::PROBE_SUFFIX, i);
		return *this;
	}

	template<typename... Types>
	auto &ReadFile(Types... path) noexcept {
		return StringPacket(TranslationCommand::READ_FILE, path...);
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
		auto MountHome(Types... mnt) noexcept {
			response.StringPacket(TranslationCommand::MOUNT_HOME,
					      mnt...);
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
		auto Home(Types... value) noexcept {
			response.StringPacket(TranslationCommand::HOME,
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

		auto CgroupNamespace() noexcept {
			response.Packet(TranslationCommand::CGROUP_NAMESPACE);
			return *this;
		}

		auto NetworkNamespace() noexcept {
			response.Packet(TranslationCommand::NETWORK_NAMESPACE);
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

		auto ForbidUserNamespace() noexcept {
			response.Packet(TranslationCommand::FORBID_USER_NS);
			return *this;
		}

		auto ForbidMulticast() noexcept {
			response.Packet(TranslationCommand::FORBID_MULTICAST);
			return *this;
		}

		auto NoNewPrivs() noexcept {
			response.Packet(TranslationCommand::NO_NEW_PRIVS);
			return *this;
		}
	};

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
	};

	template<typename... Types>
	HttpContext Http(Types... url) noexcept {
		StringPacket(TranslationCommand::HTTP, url...);
		return HttpContext(*this);
	}

	WritableBuffer<uint8_t> Finish() noexcept;

private:
	void Grow(size_t new_capacity) noexcept;
	void *Write(size_t nbytes) noexcept;

	void *WriteHeader(TranslationCommand cmd,
			  size_t payload_size) noexcept;

	static constexpr size_t GetParamLength(ConstBuffer<void> b) noexcept {
		return b.size;
	}

	static constexpr size_t GetParamLength(std::string_view sv) noexcept {
		return sv.size();
	}

	static void *WriteParam(void *dest, ConstBuffer<void> src) noexcept {
		return mempcpy(dest, src.data, src.size);
	}

	static void *WriteParam(void *dest, std::string_view src) noexcept {
		return mempcpy(dest, src.data(), src.size());
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
