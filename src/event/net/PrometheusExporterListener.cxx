// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "PrometheusExporterListener.hxx"
#include "PrometheusExporterHandler.hxx"
#include "net/SocketAddress.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "io/Iovec.hxx"
#include "util/DeleteDisposer.hxx"
#include "util/SpanCast.hxx"

#include <fmt/core.h>

#include <array>

PrometheusExporterListener::PrometheusExporterListener(EventLoop &event_loop,
						       UniqueSocketDescriptor _fd,
						       PrometheusExporterHandler &_handler) noexcept
	:ServerSocket(event_loop, std::move(_fd)),
	 handler(_handler) {}

PrometheusExporterListener::~PrometheusExporterListener() noexcept
{
	connections.clear_and_dispose(DeleteDisposer{});
}

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

class PrometheusExporterListener::Connection : public IntrusiveListHook<IntrusiveHookMode::AUTO_UNLINK>
{
	PrometheusExporterHandler &handler;
	SocketEvent socket;

public:
	Connection(EventLoop &event_loop, UniqueSocketDescriptor &&_socket,
		   PrometheusExporterHandler &_handler) noexcept
		:handler(_handler),
		 socket(event_loop, BIND_THIS_METHOD(OnSocketReady),
			_socket.Release())
	{
		socket.ScheduleRead();
	}

	~Connection() noexcept {
		socket.Close();
	}

private:
	void OnSocketReady(unsigned events) noexcept;
};

inline void
PrometheusExporterListener::Connection::OnSocketReady(unsigned events) noexcept
{
	if (events == SocketEvent::READ) {
		/* don't bother to read the HTTP request, just send
		   the response and be done */

		try {
			const auto response = handler.OnPrometheusExporterRequest();
			const auto fd = socket.GetSocket();
			SendTextResponse(fd, response);

			/* flush all pending data, do not reset the
			   TCP connection */
			fd.ShutdownWrite();
		} catch (...) {
			handler.OnPrometheusExporterError(std::current_exception());
		}
	}

	delete this;
}

void
PrometheusExporterListener::OnAccept(UniqueSocketDescriptor fd,
				     [[maybe_unused]] SocketAddress address) noexcept
{
	auto *connection = new Connection(GetEventLoop(), std::move(fd),
					  handler);
	connections.push_back(*connection);
}

void
PrometheusExporterListener::OnAcceptError(std::exception_ptr error) noexcept
{
	handler.OnPrometheusExporterError(std::move(error));
}
