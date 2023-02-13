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

class BengControlClient {
	UniqueSocketDescriptor socket;

public:
	explicit BengControlClient(UniqueSocketDescriptor _socket) noexcept
		:socket(std::move(_socket)) {}

	explicit BengControlClient(const char *host_and_port);

	void AutoBind() const noexcept {
		socket.AutoBind();
	}

	void Send(BengProxy::ControlCommand cmd,
		  std::span<const std::byte> payload={},
		  std::span<const FileDescriptor> fds={}) const;

	void Send(BengProxy::ControlCommand cmd, std::nullptr_t,
		  std::span<const FileDescriptor> fds={}) const {
		Send(cmd, std::span<const std::byte>{}, fds);
	}

	void Send(BengProxy::ControlCommand cmd, std::string_view payload,
		  std::span<const FileDescriptor> fds={}) const {
		Send(cmd, AsBytes(payload), fds);
	}

	void Send(std::span<const std::byte> payload) const;

	std::pair<BengProxy::ControlCommand, std::string> Receive() const;

	static std::string MakeTcacheInvalidate(TranslationCommand cmd,
						std::span<const std::byte> payload) noexcept;

	static std::string MakeTcacheInvalidate(TranslationCommand cmd,
						std::string_view value) noexcept {
		return MakeTcacheInvalidate(cmd, AsBytes(value));
	}
};
