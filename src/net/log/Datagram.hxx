/*
 * Copyright 2007-2022 CM4all GmbH
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

#include "Protocol.hxx"
#include "Chrono.hxx"
#include "http/Method.h"
#include "http/Status.h"
#include "util/StringView.hxx"

#include <chrono>

#include <stdint.h>

namespace Net {
namespace Log {

struct Datagram {
	TimePoint timestamp = TimePoint();

	const char *remote_host = nullptr, *host = nullptr, *site = nullptr;

	const char *forwarded_to = nullptr;

	const char *http_uri = nullptr, *http_referer = nullptr;
	const char *user_agent = nullptr;

	StringView message = nullptr;

	uint64_t length;

	uint64_t traffic_received, traffic_sent;

	Duration duration;

	http_method_t http_method = HTTP_METHOD_NULL;

	http_status_t http_status = http_status_t(0);

	Type type = Type::UNSPECIFIED;

	bool valid_length = false, valid_traffic = false;
	bool valid_duration = false;

	Datagram() = default;

	Datagram(TimePoint _timestamp,
		 http_method_t _method, const char *_uri,
		 const char *_remote_host,
		 const char *_host, const char *_site,
		 const char *_referer, const char *_user_agent,
		 http_status_t _status, int64_t _length,
		 uint64_t _traffic_received, uint64_t _traffic_sent,
		 Duration _duration) noexcept
		:timestamp(_timestamp),
		 remote_host(_remote_host), host(_host), site(_site),
		 http_uri(_uri), http_referer(_referer), user_agent(_user_agent),
		 length(_length),
		 traffic_received(_traffic_received), traffic_sent(_traffic_sent),
		 duration(_duration),
		 http_method(_method),
		 http_status(_status),
		 valid_length(_length >= 0), valid_traffic(true),
		 valid_duration(true) {}

	explicit Datagram(StringView _message) noexcept
		:message(_message) {}

	bool HasTimestamp() const noexcept {
		return timestamp != TimePoint();
	}

	void SetTimestamp(TimePoint t) noexcept {
		timestamp = t;
	}

	void SetTimestamp(std::chrono::system_clock::time_point t) noexcept {
		SetTimestamp(FromSystem(t));
	}

	bool HasHttpMethod() const noexcept {
		return http_method != HTTP_METHOD_NULL;
	}

	bool HasHttpStatus() const noexcept {
		return http_status != http_status_t(0);
	}

	bool GuessIsHttpAccess() const noexcept {
		return http_uri != nullptr && HasHttpStatus();
	}
};

}}
