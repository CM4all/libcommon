// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <stdexcept>

class SocketProtocolError : public std::runtime_error {
public:
	SocketProtocolError(const char *msg) noexcept
		:std::runtime_error(msg) {}
};

class SocketClosedPrematurelyError : public SocketProtocolError {
public:
	SocketClosedPrematurelyError() noexcept
		:SocketProtocolError("Peer closed the socket prematurely") {}
};

class SocketBufferFullError : public SocketProtocolError {
public:
	SocketBufferFullError() noexcept
		:SocketProtocolError("Socket buffer overflow") {}
};
