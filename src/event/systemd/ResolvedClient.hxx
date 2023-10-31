// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <exception>
#include <span>
#include <string_view>

class SocketAddress;
class EventLoop;
class CancellablePointer;

namespace Systemd {

class ResolveHostnameHandler {
public:
	virtual void OnResolveHostname(std::span<const SocketAddress> addresses) noexcept = 0;
	virtual void OnResolveHostnameError(std::exception_ptr error) noexcept = 0;
};

/**
 * Asynchronous client for systemd-resolved via
 * /run/systemd/resolve/io.systemd.Resolve
 */
void
ResolveHostname(EventLoop &event_loop,
		std::string_view hostname, unsigned port, int family,
		ResolveHostnameHandler &handler,
		CancellablePointer &cancel_ptr) noexcept;

} // namespace Systemd
