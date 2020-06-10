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

#include <string_view>
#include <utility>

#include <stdint.h>
#include <stddef.h>
#include <string.h>

namespace Translation::Server {

class Response {
	uint8_t *buffer = nullptr;
	size_t capacity = 0, size = 0;

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
		 size(other.size) {}

	~Response() noexcept {
		delete[] buffer;
	}

	Response &operator=(Response &&src) noexcept {
		using std::swap;
		swap(buffer, src.buffer);
		swap(capacity, src.capacity);
		swap(size, src.size);
		return *this;
	}

	/**
	 * Append an empty packet.
	 */
	void Packet(TranslationCommand cmd) noexcept {
		WriteHeader(cmd, 0);
	}

	void Packet(TranslationCommand cmd,
		    ConstBuffer<void> payload) noexcept;

	void Packet(TranslationCommand cmd,
		    std::string_view payload) noexcept {
		Packet(cmd, ConstBuffer<void>{payload.data(), payload.size()});
	}

	void Packet(TranslationCommand cmd,
		    const void *payload, size_t length) noexcept {
		Packet(cmd, {payload, length});
	}

	/**
	 * Append a packet by copying the raw bytes of an object.
	 */
	template<typename T>
	void PacketT(TranslationCommand cmd, const T &payload) noexcept {
		Packet(cmd, &payload, sizeof(payload));
	}

	/**
	 * Append a packet whose payload is a concatenation of all
	 * parameters.
	 */
	template<typename... Params>
	void MultiPacket(TranslationCommand cmd, Params... params) noexcept {
		size_t total_length = (... + GetParamLength(params));
		void *p = WriteHeader(cmd, total_length);
		WriteParams(p, params...);
	}

	void Base(const char *payload) noexcept {
		Packet(TranslationCommand::BASE, payload);
	}

	void UnsafeBase() noexcept {
		Packet(TranslationCommand::UNSAFE_BASE);
	}

	void EasyBase() noexcept {
		Packet(TranslationCommand::EASY_BASE);
	}

	void Regex(const char *payload) noexcept {
		Packet(TranslationCommand::REGEX, payload);
	}

	void InverseRegex(const char *payload) noexcept {
		Packet(TranslationCommand::INVERSE_REGEX, payload);
	}

	void RegexTail() noexcept {
		Packet(TranslationCommand::REGEX_TAIL);
	}

	void RegexUnescape() noexcept {
		Packet(TranslationCommand::REGEX_UNESCAPE);
	}

	void InverseRegexUnescape() noexcept {
		Packet(TranslationCommand::INVERSE_REGEX_UNESCAPE);
	}

	void Status(http_status_t _status) noexcept {
		uint16_t status = uint16_t(_status);
		Packet(TranslationCommand::STATUS, &status, sizeof(status));
	}

	class ProcessorContext {
		Response &response;

	public:
		constexpr explicit
		ProcessorContext(Response &_response) noexcept
			:response(_response) {}

		void Container() noexcept {
			response.Packet(TranslationCommand::CONTAINER);
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

		void PivotRoot(const char *path) noexcept {
			response.Packet(TranslationCommand::PIVOT_ROOT, path);
		}

		void MountProc() noexcept {
			response.Packet(TranslationCommand::MOUNT_PROC);
		}

		void MountTmpTmpfs() noexcept {
			response.Packet(TranslationCommand::MOUNT_TMP_TMPFS);
		}

		void MountHome(const char *mnt) noexcept {
			response.Packet(TranslationCommand::MOUNT_HOME, mnt);
		}
	};

	class ChildContext {
	protected:
		Response &response;

	public:
		constexpr explicit
		ChildContext(Response &_response) noexcept
			:response(_response) {}

		void Home(const char *value) noexcept {
			response.Packet(TranslationCommand::HOME, value);
		}

		void UserNamespace() noexcept {
			response.Packet(TranslationCommand::USER_NAMESPACE);
		}

		void PidNamespace() noexcept {
			response.Packet(TranslationCommand::PID_NAMESPACE);
		}

		void NetworkNamespace() noexcept {
			response.Packet(TranslationCommand::NETWORK_NAMESPACE);
		}

		void UtsNamespace() noexcept {
			response.Packet(TranslationCommand::PID_NAMESPACE);
		}

		MountNamespaceContext MountNamespace() noexcept {
			return MountNamespaceContext(response);
		}
	};

	class CgiAlikeChildContext : public ChildContext {
	public:
		using ChildContext::ChildContext;

		void Uri(const char *payload) noexcept {
			response.Packet(TranslationCommand::URI, payload);
		}

		void ScriptName(const char *payload) noexcept {
			response.Packet(TranslationCommand::SCRIPT_NAME, payload);
		}

		void PathInfo(const char *payload) noexcept {
			response.Packet(TranslationCommand::PATH_INFO, payload);
		}

		void QueryString(const char *payload) noexcept {
			response.Packet(TranslationCommand::QUERY_STRING, payload);
		}
	};

	class WasChildContext : public CgiAlikeChildContext {
	public:
		using CgiAlikeChildContext::CgiAlikeChildContext;

		void Parameter(const char *s) noexcept {
			response.Packet(TranslationCommand::PAIR, s);
		}
	};

	WasChildContext Was(const char *path) noexcept {
		Packet(TranslationCommand::WAS, path);
		return WasChildContext(*this);
	}

	class FileContext {
		Response &response;

	public:
		constexpr explicit
		FileContext(Response &_response) noexcept
			:response(_response) {}

		void ExpandPath(const char *value) noexcept {
			response.Packet(TranslationCommand::EXPAND_PATH, value);
		}

		void ContentType(const char *value) noexcept {
			response.Packet(TranslationCommand::CONTENT_TYPE, value);
		}

		void Deflated(const char *path) noexcept {
			response.Packet(TranslationCommand::DEFLATED, path);
		}

		void Gzipped(const char *path) noexcept {
			response.Packet(TranslationCommand::GZIPPED, path);
		}

		void DocumentRoot(const char *value) noexcept {
			response.Packet(TranslationCommand::DOCUMENT_ROOT, value);
		}

		ChildContext Delegate(const char *helper) noexcept {
			response.Packet(TranslationCommand::DELEGATE, helper);
			return ChildContext(response);
		}
	};

	auto Path(const char *path) noexcept {
		Packet(TranslationCommand::PATH, path);
		return FileContext(*this);
	}

	class HttpContext {
		Response &response;

	public:
		constexpr explicit
		HttpContext(Response &_response) noexcept
			:response(_response) {}

		void ExpandPath(const char *value) noexcept {
			response.Packet(TranslationCommand::EXPAND_PATH, value);
		}

		void Address(SocketAddress address) noexcept {
			response.Packet(TranslationCommand::ADDRESS,
					address.GetAddress(),
					address.GetSize());
		}

		template<typename A>
		void Addresses(const A &addresses) noexcept {
			for (const auto &i : addresses)
				Address(i);
		}
	};

	HttpContext Http(const char *url) noexcept {
		Packet(TranslationCommand::HTTP, url);
		return HttpContext(*this);
	}

	WritableBuffer<uint8_t> Finish() noexcept {
		Packet(TranslationCommand::END);

		WritableBuffer<uint8_t> result(buffer, size);
		buffer = nullptr;
		capacity = size = 0;
		return result;
	}

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
