// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "PrometheusExporterListener.hxx"
#include "PrometheusExporterHandler.hxx"
#include "net/SocketAddress.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "io/Iovec.hxx"
#include "util/SpanCast.hxx"

#include <fmt/core.h>

#include <array>

PrometheusExporterListener::PrometheusExporterListener(EventLoop &event_loop,
						       UniqueSocketDescriptor _fd,
						       PrometheusExporterHandler &_handler) noexcept
	:ServerSocket(event_loop, std::move(_fd)),
	 handler(_handler) {}

static void
SendTextResponse(SocketDescriptor socket, std::string_view body) noexcept
{
	const auto headers = fmt::format("HTTP/1.1 200 OK\r\n"
					 "connection: close\r\n"
					 "content-type: text/plain\r\n"
					 "content-length: {}\r\n"
					 "\r\n",
					 body.size());

	(void)socket.Send(std::array{MakeIovec(AsBytes(headers)), MakeIovec(AsBytes(body))});
}

void
PrometheusExporterListener::OnAccept(UniqueSocketDescriptor fd,
				     [[maybe_unused]] SocketAddress address) noexcept
try {
	const auto response = handler.OnPrometheusExporterRequest();
	SendTextResponse(fd, response);
	fd.ShutdownWrite();
} catch (...) {
	handler.OnPrometheusExporterError(std::current_exception());
}

void
PrometheusExporterListener::OnAcceptError(std::exception_ptr error) noexcept
{
	handler.OnPrometheusExporterError(std::move(error));
}
