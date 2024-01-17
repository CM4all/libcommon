// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Protocol.hxx"
#include "translation/Protocol.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "util/SpanCast.hxx"

#include <span>
#include <string>
#include <string_view>

namespace BengControl {

class Client {
	UniqueSocketDescriptor socket;

public:
	explicit Client(UniqueSocketDescriptor _socket) noexcept
		:socket(std::move(_socket)) {}

	explicit Client(const char *host_and_port);

	void AutoBind() const noexcept {
		socket.AutoBind();
	}

	void Send(Command cmd,
		  std::span<const std::byte> payload={},
		  std::span<const FileDescriptor> fds={}) const;

	void Send(Command cmd, std::nullptr_t,
		  std::span<const FileDescriptor> fds={}) const {
		Send(cmd, std::span<const std::byte>{}, fds);
	}

	void Send(Command cmd, std::string_view payload,
		  std::span<const FileDescriptor> fds={}) const {
		Send(cmd, AsBytes(payload), fds);
	}

	void Send(std::span<const std::byte> payload) const;

	std::pair<Command, std::string> Receive() const;

	static std::string MakeTcacheInvalidate(TranslationCommand cmd,
						std::span<const std::byte> payload) noexcept;

	static std::string MakeTcacheInvalidate(TranslationCommand cmd,
						std::string_view value) noexcept {
		return MakeTcacheInvalidate(cmd, AsBytes(value));
	}
};

} // namespace BengControl
