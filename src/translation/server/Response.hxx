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

	auto &Packet(TranslationCommand cmd,
		     std::string_view payload) noexcept {
		return Packet(cmd, ConstBuffer<void>{payload.data(), payload.size()});
	}

	auto &Packet(TranslationCommand cmd,
		     const void *payload, size_t length) noexcept {
		return Packet(cmd, {payload, length});
	}

	/**
	 * Append a packet by copying the raw bytes of an object.
	 */
	template<typename T>
	auto &PacketT(TranslationCommand cmd, const T &payload) noexcept {
		return Packet(cmd, &payload, sizeof(payload));
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

	auto &Token(std::string_view value) noexcept {
		return Packet(TranslationCommand::TOKEN, value);
	}

	auto &Base(std::string_view payload) noexcept {
		return Packet(TranslationCommand::BASE, payload);
	}

	auto &UnsafeBase() noexcept {
		return Packet(TranslationCommand::UNSAFE_BASE);
	}

	auto &EasyBase() noexcept {
		return Packet(TranslationCommand::EASY_BASE);
	}

	auto &Regex(std::string_view payload) noexcept {
		return Packet(TranslationCommand::REGEX, payload);
	}

	auto &InverseRegex(std::string_view payload) noexcept {
		return Packet(TranslationCommand::INVERSE_REGEX, payload);
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

	auto &Status(http_status_t _status) noexcept {
		uint16_t status = uint16_t(_status);
		return Packet(TranslationCommand::STATUS,
			      &status, sizeof(status));
	}

	auto &Site(std::string_view value) noexcept {
		return Packet(TranslationCommand::SITE, value);
	}

	auto &CanonicalHost(std::string_view value) noexcept {
		return Packet(TranslationCommand::CANONICAL_HOST, value);
	}

	struct RedirectContext {
		Response &response;

		auto CopyQueryString() noexcept {
			response.Packet(TranslationCommand::REDIRECT_QUERY_STRING);
			return *this;
		}
	};

	auto Redirect(std::string_view value) noexcept {
		Packet(TranslationCommand::REDIRECT, value);
		return RedirectContext{*this};
	}

	auto ExpandRedirect(std::string_view value) noexcept {
		Packet(TranslationCommand::EXPAND_REDIRECT, value);
		return RedirectContext{*this};
	}

	auto &Bounce(std::string_view value) noexcept {
		return Packet(TranslationCommand::BOUNCE, value);
	}

	auto &TestPath(std::string_view value) noexcept {
		return Packet(TranslationCommand::TEST_PATH, value);
	}

	auto &ExpandTestPath(std::string_view value) noexcept {
		return Packet(TranslationCommand::EXPAND_TEST_PATH, value);
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

		auto PivotRoot(std::string_view path) noexcept {
			response.Packet(TranslationCommand::PIVOT_ROOT, path);
			return *this;
		}

		auto MountProc() noexcept {
			response.Packet(TranslationCommand::MOUNT_PROC);
			return *this;
		}

		auto MountTmpTmpfs() noexcept {
			response.Packet(TranslationCommand::MOUNT_TMP_TMPFS);
			return *this;
		}

		auto MountHome(std::string_view mnt) noexcept {
			response.Packet(TranslationCommand::MOUNT_HOME, mnt);
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

		auto SetEnv(std::string_view s) noexcept {
			response.Packet(TranslationCommand::SETENV, s);
			return *this;
		}

		auto SetEnv(std::string_view name,
			    std::string_view value) noexcept {
			response.MultiPacket(TranslationCommand::SETENV,
					     name, "=", value);
			return *this;
		}

		auto ExpandSetEnv(std::string_view s) noexcept {
			response.Packet(TranslationCommand::EXPAND_SETENV, s);
			return *this;
		}

		auto ExpandSetEnv(std::string_view name,
				  std::string_view value) noexcept {
			response.MultiPacket(TranslationCommand::EXPAND_SETENV,
					     name, "=", value);
			return *this;
		}

		auto Append(std::string_view value) noexcept {
			response.Packet(TranslationCommand::APPEND, value);
			return *this;
		}

		auto ExpandAppend(std::string_view value) noexcept {
			response.Packet(TranslationCommand::EXPAND_APPEND, value);
			return *this;
		}

		auto Home(std::string_view value) noexcept {
			response.Packet(TranslationCommand::HOME, value);
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

		auto NetworkNamespace() noexcept {
			response.Packet(TranslationCommand::NETWORK_NAMESPACE);
			return *this;
		}

		auto UtsNamespace() noexcept {
			response.Packet(TranslationCommand::PID_NAMESPACE);
			return *this;
		}

		MountNamespaceContext MountNamespace() noexcept {
			return MountNamespaceContext(response);
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

		auto ExpandPath(std::string_view payload) noexcept {
			response.Packet(TranslationCommand::EXPAND_PATH, payload);
			return *this;
		}

		auto Action(std::string_view payload) noexcept {
			response.Packet(TranslationCommand::ACTION, payload);
			return *this;
		}

		auto Uri(std::string_view payload) noexcept {
			response.Packet(TranslationCommand::URI, payload);
			return *this;
		}

		auto ScriptName(std::string_view payload) noexcept {
			response.Packet(TranslationCommand::SCRIPT_NAME, payload);
			return *this;
		}

		auto ExpandScriptName(std::string_view payload) noexcept {
			response.Packet(TranslationCommand::EXPAND_SCRIPT_NAME,
					payload);
			return *this;
		}

		auto PathInfo(std::string_view payload) noexcept {
			response.Packet(TranslationCommand::PATH_INFO, payload);
			return *this;
		}

		auto ExpandPathInfo(std::string_view payload) noexcept {
			response.Packet(TranslationCommand::EXPAND_PATH_INFO, payload);
			return *this;
		}

		auto QueryString(std::string_view payload) noexcept {
			response.Packet(TranslationCommand::QUERY_STRING, payload);
			return *this;
		}
	};

	class WasChildContext : public CgiAlikeChildContext {
	public:
		using CgiAlikeChildContext::CgiAlikeChildContext;

		auto Parameter(std::string_view s) noexcept {
			response.Packet(TranslationCommand::PAIR, s);
			return *this;
		}

		auto Parameter(std::string_view name,
			       std::string_view value) noexcept {
			response.MultiPacket(TranslationCommand::PAIR,
					     name, "=", value);
			return *this;
		}

		auto ExpandParameter(std::string_view s) noexcept {
			response.Packet(TranslationCommand::EXPAND_PAIR, s);
			return *this;
		}

		auto ExpandParameter(std::string_view name,
				     std::string_view value) noexcept {
			response.MultiPacket(TranslationCommand::EXPAND_PAIR,
					     name, "=", value);
			return *this;
		}
	};

	WasChildContext Was(std::string_view path) noexcept {
		Packet(TranslationCommand::WAS, path);
		return WasChildContext(*this);
	}

	class FastCgiChildContext : public CgiAlikeChildContext {
	public:
		using CgiAlikeChildContext::CgiAlikeChildContext;

		auto Parameter(std::string_view s) noexcept {
			response.Packet(TranslationCommand::PAIR, s);
			return *this;
		}

		auto Parameter(std::string_view name,
			       std::string_view value) noexcept {
			response.MultiPacket(TranslationCommand::PAIR,
					     name, "=", value);
			return *this;
		}

		auto ExpandParameter(std::string_view s) noexcept {
			response.Packet(TranslationCommand::EXPAND_PAIR, s);
			return *this;
		}

		auto DocumentRoot(std::string_view value) noexcept {
			response.Packet(TranslationCommand::DOCUMENT_ROOT, value);
			return *this;
		}

		auto ExpandDocumentRoot(std::string_view value) noexcept {
			response.Packet(TranslationCommand::EXPAND_DOCUMENT_ROOT, value);
			return *this;
		}
	};

	FastCgiChildContext FastCGI(std::string_view path) noexcept {
		Packet(TranslationCommand::FASTCGI, path);
		return FastCgiChildContext(*this);
	}

	class CgiChildContext : public CgiAlikeChildContext {
	public:
		using CgiAlikeChildContext::CgiAlikeChildContext;

		auto Interpreter(std::string_view payload) noexcept {
			response.Packet(TranslationCommand::INTERPRETER, payload);
			return *this;
		}

		auto DocumentRoot(std::string_view value) noexcept {
			response.Packet(TranslationCommand::DOCUMENT_ROOT, value);
			return *this;
		}

		auto ExpandDocumentRoot(std::string_view value) noexcept {
			response.Packet(TranslationCommand::EXPAND_DOCUMENT_ROOT, value);
			return *this;
		}
	};

	CgiChildContext CGI(std::string_view path) noexcept {
		Packet(TranslationCommand::CGI, path);
		return CgiChildContext(*this);
	}

	class FileContext {
		Response &response;

	public:
		constexpr explicit
		FileContext(Response &_response) noexcept
			:response(_response) {}

		auto ExpandPath(std::string_view value) noexcept {
			response.Packet(TranslationCommand::EXPAND_PATH, value);
			return *this;
		}

		auto ContentType(std::string_view value) noexcept {
			response.Packet(TranslationCommand::CONTENT_TYPE, value);
			return *this;
		}

		auto Deflated(std::string_view path) noexcept {
			response.Packet(TranslationCommand::DEFLATED, path);
			return *this;
		}

		auto Gzipped(std::string_view path) noexcept {
			response.Packet(TranslationCommand::GZIPPED, path);
			return *this;
		}

		auto DocumentRoot(std::string_view value) noexcept {
			response.Packet(TranslationCommand::DOCUMENT_ROOT, value);
			return *this;
		}

		auto ExpandDocumentRoot(std::string_view value) noexcept {
			response.Packet(TranslationCommand::EXPAND_DOCUMENT_ROOT, value);
			return *this;
		}

		ChildContext Delegate(std::string_view helper) noexcept {
			response.Packet(TranslationCommand::DELEGATE, helper);
			return ChildContext(response);
		}
	};

	auto Path(std::string_view path) noexcept {
		Packet(TranslationCommand::PATH, path);
		return FileContext(*this);
	}

	class HttpContext {
		Response &response;

	public:
		constexpr explicit
		HttpContext(Response &_response) noexcept
			:response(_response) {}

		auto ExpandPath(std::string_view value) noexcept {
			response.Packet(TranslationCommand::EXPAND_PATH, value);
			return *this;
		}

		auto Address(SocketAddress address) noexcept {
			response.Packet(TranslationCommand::ADDRESS,
					address.GetAddress(),
					address.GetSize());
			return *this;
		}

		template<typename A>
		auto Addresses(const A &addresses) noexcept {
			for (const auto &i : addresses)
				Address(i);
			return *this;
		}
	};

	HttpContext Http(std::string_view url) noexcept {
		Packet(TranslationCommand::HTTP, url);
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
	static void *WriteParams(void *dest, P first, Params... params) noexcept {
		dest = WriteParam(dest, first);
		return WriteParams(dest, params...);
	}

	static void *WriteParams(void *dest) noexcept {
		return dest;
	}
};

} // namespace Translation::Server
