// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <exception>
#include <span>
#include <string_view>

class EventLoop;
class CancellablePointer;
union InetAddress;

namespace Systemd {

class ResolveHostnameHandler {
public:
	virtual void OnResolveHostname(std::span<const InetAddress> addresses) noexcept = 0;
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
