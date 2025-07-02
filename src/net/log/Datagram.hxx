// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "Protocol.hxx"
#include "Chrono.hxx"

#include <chrono>
#include <cstdint>
#include <string_view>

enum class HttpMethod : uint_least8_t;
enum class HttpStatus : uint_least16_t;

namespace Net::Log {

enum class ContentType : uint8_t;

struct Datagram {
	TimePoint timestamp = TimePoint();

	const char *remote_host = nullptr, *host = nullptr, *site = nullptr;

	const char *analytics_id = nullptr;

	const char *generator = nullptr;

	const char *forwarded_to = nullptr;

	std::string_view http_uri{}, http_referer{};
	std::string_view user_agent{};

	std::string_view message{};

	std::string_view json{};

	uint_least64_t length;

	uint_least64_t traffic_received, traffic_sent;

	Duration duration;

	HttpMethod http_method = {};

	HttpStatus http_status = {};

	Type type = Type::UNSPECIFIED;

	ContentType content_type = {};

	bool valid_length = false, valid_traffic = false;
	bool valid_duration = false;

	bool truncated_host = false;
	bool truncated_http_uri = false;
	bool truncated_http_referer = false;
	bool truncated_user_agent = false;
	bool truncated_message = false;

	constexpr bool HasTimestamp() const noexcept {
		return timestamp != TimePoint();
	}

	constexpr auto &SetTimestamp(TimePoint t) noexcept {
		timestamp = t;
		return *this;
	}

	constexpr auto &SetLength(uint_least64_t _length) noexcept {
		length = _length;
		valid_length = true;
		return *this;
	}

	constexpr auto &SetTraffic(uint_least64_t _received,
				   uint_least64_t _sent) noexcept {
		traffic_received = _received;
		traffic_sent = _sent;
		valid_traffic = true;
		return *this;
	}

	constexpr auto &SetDuration(Duration _duration) noexcept {
		duration = _duration;
		valid_duration = true;
		return *this;
	}

	constexpr auto &SetTimestamp(std::chrono::system_clock::time_point t) noexcept {
		return SetTimestamp(FromSystem(t));
	}

	constexpr bool HasHttpMethod() const noexcept {
		return http_method != HttpMethod{};
	}

	constexpr bool HasHttpStatus() const noexcept {
		return http_status != HttpStatus{};
	}

	constexpr bool GuessIsHttpAccess() const noexcept {
		return http_uri.data() != nullptr && HasHttpMethod() &&
			/* the following matches cancelled HTTP
			   requests (that have no HTTP status), but
			   rejects HTTP error messages (via
			   #valid_traffic; HTTP error messages have no
			   traffic) */
			(HasHttpStatus() || valid_traffic);
	}

	constexpr bool IsHttpAccess() const noexcept {
		return type == Type::HTTP_ACCESS ||
			(type == Type::UNSPECIFIED && GuessIsHttpAccess());
	}

	constexpr void TruncateHttpUri(std::size_t max_length) noexcept {
		Truncate(http_uri, truncated_http_uri, max_length);
	}

	constexpr void TruncateHttpReferer(std::size_t max_length) noexcept {
		Truncate(http_referer, truncated_http_referer, max_length);
	}

	constexpr void TruncateUserAgent(std::size_t max_length) noexcept {
		Truncate(user_agent, truncated_user_agent, max_length);
	}

	constexpr void TruncateMessage(std::size_t max_length) noexcept {
		Truncate(message, truncated_message, max_length);
	}

private:
	static constexpr void Truncate(std::string_view &value, bool &truncated_flag,
				       std::size_t max_length) noexcept {
		if (value.size() > max_length) {
			value = value.substr(0, max_length);
			truncated_flag = true;
		}
	}
};

} // namespace Net::Log
