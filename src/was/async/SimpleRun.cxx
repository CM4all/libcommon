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

#include "SimpleRun.hxx"
#include "SimpleServer.hxx"
#include "SimpleMultiServer.hxx"
#include "event/Loop.hxx"
#include "event/ShutdownListener.hxx"
#include "util/Cast.hxx"
#include "util/DeleteDisposer.hxx"
#include "util/IntrusiveList.hxx"

#ifdef HAVE_LIBSYSTEMD
#include "event/net/ServerSocket.hxx"
#include "net/SocketAddress.hxx"
#include <systemd/sd-daemon.h>
#include <forward_list>
#endif

#include <unistd.h>

namespace Was {
namespace {

class RunConnectionHandler final : public SimpleServerHandler {
	EventLoop &event_loop;

	std::exception_ptr error;

public:
	explicit RunConnectionHandler(EventLoop &_event_loop) noexcept
		:event_loop(_event_loop) {}

	void OnWasError(Was::SimpleServer &,
			std::exception_ptr _error) noexcept override {
		error = std::move(_error);
		event_loop.Break();
	}

	void OnWasClosed(Was::SimpleServer &) noexcept override {
		event_loop.Break();
	}

	void CheckRethrowError() const {
		if (error)
			std::rethrow_exception(error);
	}
};

static void
RunSingle(EventLoop &event_loop, SimpleRequestHandler &request_handler)
{
	RunConnectionHandler connection_handler(event_loop);
	Was::SimpleServer s(event_loop,
			    {
				    UniqueSocketDescriptor{3},
				    UniqueFileDescriptor(STDIN_FILENO),
				    UniqueFileDescriptor(STDOUT_FILENO),
			    },
			    connection_handler,
			    request_handler);

	event_loop.Run();
	connection_handler.CheckRethrowError();
}

class ConnectionList final
	: SimpleServerHandler
{
	struct Connection final : AutoUnlinkIntrusiveListHook {
		SimpleServer server;

		Connection(EventLoop &event_loop, WasSocket &&socket,
			   SimpleServerHandler &_handler,
			   SimpleRequestHandler &_request_handler) noexcept
			:server(event_loop, std::move(socket),
				_handler, _request_handler) {}
	};

	SimpleRequestHandler &request_handler;

	IntrusiveList<Connection> connections;

public:
	explicit ConnectionList(SimpleRequestHandler &_request_handler) noexcept
		:request_handler(_request_handler) {}

	~ConnectionList() noexcept {
		connections.clear_and_dispose(DeleteDisposer{});
	}

	void Add(EventLoop &event_loop, WasSocket &&socket) noexcept {
		auto *connection = new Connection(event_loop,
						  std::move(socket),
						  *this, request_handler);
		connections.push_back(*connection);
	}

private:
	/* virtual methods from class SimpleServerHandler */
	void OnWasError(Was::SimpleServer &_connection,
			std::exception_ptr) noexcept override {
		// TODO log error?
		auto *connection = &ContainerCast(_connection,
						  &Connection::server);
		delete connection;
	}

	void OnWasClosed(Was::SimpleServer &_connection) noexcept override {
		auto *connection = &ContainerCast(_connection,
						  &Connection::server);
		delete connection;
	}
};

class MultiRunServer final
	: SimpleMultiServerHandler
{
	SimpleMultiServer server;

	std::exception_ptr error;

	ConnectionList connections;

public:
	MultiRunServer(EventLoop &event_loop, UniqueSocketDescriptor &&s,
		       SimpleRequestHandler &_request_handler) noexcept
		:server(event_loop, std::move(s), *this),
		 connections(_request_handler) {}

	auto &GetEventLoop() const noexcept {
		return server.GetEventLoop();
	}

	void CheckRethrowError() const {
		if (error)
			std::rethrow_exception(error);
	}

private:
	/* virtual methods from class SimpleMultiServerHandler */
	void OnMultiWasNew(SimpleMultiServer &,
			   WasSocket &&socket) noexcept override {
		connections.Add(GetEventLoop(), std::move(socket));
	}

	void OnMultiWasError(SimpleMultiServer &,
			     std::exception_ptr _error) noexcept override {
		error = std::move(_error);
		GetEventLoop().Break();
	}

	void OnMultiWasClosed(SimpleMultiServer &) noexcept override {
		GetEventLoop().Break();
	}
};

static void
RunMulti(EventLoop &event_loop, SimpleRequestHandler &request_handler)
{
	MultiRunServer server{event_loop, UniqueSocketDescriptor{STDIN_FILENO},
		request_handler};
	event_loop.Run();
	server.CheckRethrowError();
}

#ifdef HAVE_LIBSYSTEMD

class MultiConnection final
	: public AutoUnlinkIntrusiveListHook, SimpleMultiServerHandler
{
	SimpleMultiServer server;

	ConnectionList connections;

public:
	MultiConnection(EventLoop &event_loop, UniqueSocketDescriptor &&s,
			SimpleRequestHandler &_request_handler) noexcept
		:server(event_loop, std::move(s), *this),
		 connections(_request_handler) {}

	auto &GetEventLoop() const noexcept {
		return server.GetEventLoop();
	}

private:
	/* virtual methods from class SimpleMultiServerHandler */
	void OnMultiWasNew(SimpleMultiServer &,
			   WasSocket &&socket) noexcept override {
		connections.Add(GetEventLoop(), std::move(socket));
	}

	void OnMultiWasError(SimpleMultiServer &,
			     std::exception_ptr) noexcept override {
		// TODO log error?
		delete this;
	}

	void OnMultiWasClosed(SimpleMultiServer &) noexcept override {
		delete this;
	}
};

class MultiConnectionList final
{
	SimpleRequestHandler &request_handler;

	IntrusiveList<MultiConnection> connections;

public:
	explicit MultiConnectionList(SimpleRequestHandler &_request_handler) noexcept
		:request_handler(_request_handler) {}

	~MultiConnectionList() noexcept {
		connections.clear_and_dispose(DeleteDisposer{});
	}

	void Add(EventLoop &event_loop, UniqueSocketDescriptor &&s) noexcept {
		auto *connection = new MultiConnection(event_loop,
						       std::move(s),
						       request_handler);
		connections.push_back(*connection);
	}
};

class MultiListener final
	: public AutoUnlinkIntrusiveListHook, ServerSocket
{
	MultiConnectionList connections;

	std::exception_ptr error;

public:
	MultiListener(EventLoop &event_loop,
		      UniqueSocketDescriptor &&_fd,
		      SimpleRequestHandler &request_handler) noexcept
		:ServerSocket(event_loop, std::move(_fd)),
		 connections(request_handler) {}

	void CheckRethrowError() const {
		if (error)
			std::rethrow_exception(error);
	}

private:
	/* virtual methods from class ServerSocket */
	void OnAccept(UniqueSocketDescriptor &&fd,
		      SocketAddress) noexcept {
		connections.Add(GetEventLoop(), std::move(fd));
	}

	void OnAcceptError(std::exception_ptr _error) noexcept {
		error = std::move(_error);
		GetEventLoop().Break();
	}
};

static void
RunSystemd(EventLoop &event_loop, unsigned n,
	   SimpleRequestHandler &request_handler)
{
	std::forward_list<MultiListener> listeners;
	for (unsigned i = 0; i < n; ++i)
		listeners.emplace_front(event_loop,
					UniqueSocketDescriptor(3 + i),
					request_handler);

	sd_notify(0, "READY=1");

	event_loop.Run();

	for (auto &i : listeners)
		i.CheckRethrowError();
}

#endif

} // anonymous namespace

void
Run(EventLoop &event_loop, SimpleRequestHandler &request_handler)
{
	ShutdownListener shutdown_listener{
		event_loop,
		BIND_METHOD(event_loop, &EventLoop::Break),
	};
	shutdown_listener.Enable();

#ifdef HAVE_LIBSYSTEMD
	if (int n = sd_listen_fds(true); n > 0) {
		/* launched with systemd socket activation */
		RunSystemd(event_loop, n, request_handler);
		return;
	}
#endif

	/* if STDIN is a pipe, then we're running in "single" mode; if
	   not, we assume this is "multi" mode */
	if (FileDescriptor{STDIN_FILENO}.IsPipe())
		RunSingle(event_loop, request_handler);
	else
		RunMulti(event_loop, request_handler);
}

} // namespace Was
