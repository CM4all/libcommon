// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "ServerSocket.hxx"
#include "util/IntrusiveList.hxx"

class PrometheusExporterHandler;

/**
 * Accept incoming connections and reply to simple HTTP requests.
 * This class implements just enough HTTP to be able to generate
 * Prometheus text responses.
 */
class PrometheusExporterListener final : ServerSocket {
	PrometheusExporterHandler &handler;

	class Connection;
	IntrusiveList<Connection> connections;

public:
	PrometheusExporterListener(EventLoop &event_loop,
				   UniqueSocketDescriptor _fd,
				   PrometheusExporterHandler &_handler) noexcept;
	~PrometheusExporterListener() noexcept;

	using ServerSocket::GetEventLoop;

protected:
	void OnAccept(UniqueSocketDescriptor fd,
		      SocketAddress address) noexcept override;
	void OnAcceptError(std::exception_ptr error) noexcept override;
};
