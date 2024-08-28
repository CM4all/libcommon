// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "was/async/SimpleHandler.hxx"
#include "was/async/SimpleServer.hxx"
#include "was/async/SimpleClient.hxx"
#include "was/async/Socket.hxx"
#include "event/Loop.hxx"
#include "event/FineTimerEvent.hxx"
#include "net/SocketProtocolError.hxx"

#include <gtest/gtest.h>

namespace {

class DeferBreak {
	FineTimerEvent event;

public:
	explicit DeferBreak(EventLoop &event_loop) noexcept
		:event(event_loop, BIND_THIS_METHOD(OnDeferred)) {}

	void ScheduleBreak() noexcept {
		event.Schedule(std::chrono::milliseconds{1});
	}

private:
	void OnDeferred() noexcept {
		event.GetEventLoop().Break();
	}
};

struct MyServerHandler final :  public Was::SimpleServerHandler {
	std::exception_ptr error;
	bool closed = false;

	// virtual methods from Was::SimpleServerHandler
	void OnWasError([[maybe_unused]] Was::SimpleServer &server,
			std::exception_ptr _error) noexcept override {
		assert(!error);
		assert(!closed);

		error = std::move(_error);
	}

	void OnWasClosed([[maybe_unused]] Was::SimpleServer &server) noexcept override {
		assert(!error);
		assert(!closed);

		closed = true;
	}
};

struct MyClientHandler final :  public Was::SimpleClientHandler {
	std::exception_ptr error;
	bool closed = false;

	// virtual methods from Was::SimpleClientHandler
	void OnWasError(std::exception_ptr _error) noexcept override {
		assert(!error);
		assert(!closed);

		error = std::move(_error);
	}

	void OnWasClosed() noexcept override {
		assert(!error);
		assert(!closed);

		closed = true;
	}
};

class MyResponseHandler final : public Was::SimpleResponseHandler {
	EventLoop &event_loop;

public:
	Was::SimpleResponse response;
	std::exception_ptr error;

	explicit MyResponseHandler(EventLoop &_event_loop) noexcept
		:event_loop(_event_loop) {}

	// virtual methods from Was::SimpleResponseHandler
	void OnWasResponse(Was::SimpleResponse &&_response) noexcept override {
		response = std::move(_response);
		event_loop.Break();
	}

	void OnWasError(std::exception_ptr _error) noexcept override {
		error = std::move(_error);
		event_loop.Break();
	}
};

static Was::SimpleResponse
Request(Was::SimpleClient &client, Was::SimpleRequest &&request)
{
	auto &event_loop = client.GetEventLoop();
	MyResponseHandler response_handler{event_loop};

	CancellablePointer cancel_ptr;
	client.SendRequest(std::move(request), response_handler, cancel_ptr);

	event_loop.Run();

	if (response_handler.error)
		std::rethrow_exception(response_handler.error);

	return std::move(response_handler.response);
}

class MyRequestHandler final : public Was::SimpleRequestHandler, Cancellable {
	EventLoop &event_loop;

public:
	enum class Mode {
		MIRROR,
		DEFER,
	} mode;

	bool deferred = false;
	bool canceled = false;

	explicit MyRequestHandler(EventLoop &_event_loop, Mode _mode) noexcept
		:event_loop(_event_loop), mode(_mode) {}

	void Reset() noexcept {
		deferred = false;
		canceled = false;
	}

	// virtual methods from Was::SimpleRequestHandler
	bool OnRequest(Was::SimpleServer &server, Was::SimpleRequest &&request,
		       CancellablePointer &cancel_ptr) noexcept override;

private:
	// virtual methods from class Cancellable */
	void Cancel() noexcept override {
		canceled = true;
		event_loop.Break();
	}
};

bool
MyRequestHandler::OnRequest(Was::SimpleServer &server, Was::SimpleRequest &&request,
			    [[maybe_unused]] CancellablePointer &cancel_ptr) noexcept
{
	assert(!deferred);
	assert(!canceled);

	switch (mode) {
	case Mode::MIRROR:
		return server.SendResponse({
			.headers = std::move(request.headers),
			.body = std::move(request.body),
		});

	case Mode::DEFER:
		cancel_ptr = *this;
		deferred = true;
		event_loop.Break();
		return true;
	}

	// unreachable
	std::terminate();
}

} // anonymous namespace

TEST(WasSimpleServer, Basic)
{
	auto [for_client, for_server] = WasSocket::CreatePair();

	EventLoop event_loop;

	MyServerHandler server_handler;
	MyRequestHandler request_handler{event_loop, MyRequestHandler::Mode::MIRROR};
	Was::SimpleServer server{event_loop, std::move(for_server), server_handler, request_handler};

	MyClientHandler client_handler;
	Was::SimpleClient client{event_loop, std::move(for_client), client_handler};

	// send bare GET request without headers
	const auto response1 = Request(client, {
		.method = HttpMethod::GET,
		.uri = "/foo",
	});
	EXPECT_EQ(response1.status, HttpStatus::OK);
	EXPECT_TRUE(response1.headers.empty());
	EXPECT_FALSE(response1.body);
	EXPECT_FALSE(client_handler.closed);
	EXPECT_FALSE(client_handler.error);
	EXPECT_FALSE(server_handler.closed);
	EXPECT_FALSE(server_handler.error);

	// send GET request with one header
	const auto response2 = Request(client, {
		.method = HttpMethod::GET,
		.uri = "/foo",
		.headers = {
			{"hello", "world"},
		},
	});
	EXPECT_EQ(response2.status, HttpStatus::OK);
	EXPECT_FALSE(response2.headers.empty());
	EXPECT_FALSE(response2.body);
	EXPECT_FALSE(client_handler.closed);
	EXPECT_FALSE(client_handler.error);
	EXPECT_FALSE(server_handler.closed);
	EXPECT_FALSE(server_handler.error);

	// close the client, expect server-side OnWasClosed() call
	client.Close();
	event_loop.Run();
	EXPECT_FALSE(client_handler.closed);
	EXPECT_FALSE(client_handler.error);
	EXPECT_TRUE(server_handler.closed);
	EXPECT_FALSE(server_handler.error);
}

TEST(WasSimpleServer, Cancel)
{
	auto [for_client, for_server] = WasSocket::CreatePair();

	EventLoop event_loop;

	MyServerHandler server_handler;
	MyRequestHandler request_handler{event_loop, MyRequestHandler::Mode::DEFER};
	Was::SimpleServer server{event_loop, std::move(for_server), server_handler, request_handler};

	MyClientHandler client_handler;
	Was::SimpleClient client{event_loop, std::move(for_client), client_handler};
	MyResponseHandler response_handler{event_loop};

	CancellablePointer cancel_ptr;
	client.SendRequest({
		.method = HttpMethod::GET,
		.uri = "/foo",
	}, response_handler, cancel_ptr);
	event_loop.Run();

	EXPECT_TRUE(cancel_ptr);
	EXPECT_TRUE(request_handler.deferred);
	EXPECT_FALSE(request_handler.canceled);
	EXPECT_FALSE(client_handler.closed);
	EXPECT_FALSE(client_handler.error);
	EXPECT_FALSE(server_handler.closed);
	EXPECT_FALSE(server_handler.error);

	// client cancels
	cancel_ptr.Cancel();
	event_loop.Run();

	if (client.IsStopping()) {
		// wait some more until the server sends PREMATURE
		DeferBreak defer{event_loop};
		defer.ScheduleBreak();
		event_loop.Run();
	}

	EXPECT_TRUE(request_handler.deferred);
	EXPECT_TRUE(request_handler.canceled);
	EXPECT_FALSE(client_handler.closed);
	EXPECT_FALSE(client_handler.error);
	EXPECT_FALSE(server_handler.closed);
	EXPECT_FALSE(server_handler.error);

	// close the client, expect server-side OnWasClosed() call
	client.Close();
	event_loop.Run();
	EXPECT_FALSE(client_handler.closed);
	EXPECT_FALSE(client_handler.error);
	EXPECT_TRUE(server_handler.closed);
	EXPECT_FALSE(server_handler.error);
	if (server_handler.error)
	std::rethrow_exception(server_handler.error);
}

TEST(WasSimpleServer, ServerClose)
{
	auto [for_client, for_server] = WasSocket::CreatePair();

	EventLoop event_loop;

	MyServerHandler server_handler;
	MyRequestHandler request_handler{event_loop, MyRequestHandler::Mode::MIRROR};
	Was::SimpleServer server{event_loop, std::move(for_server), server_handler, request_handler};

	MyClientHandler client_handler;
	Was::SimpleClient client{event_loop, std::move(for_client), client_handler};

	server.Close();
	event_loop.Run();

	EXPECT_FALSE(client_handler.closed);
	EXPECT_TRUE(client_handler.error);
	EXPECT_FALSE(server_handler.closed);
	EXPECT_FALSE(server_handler.error);

	EXPECT_THROW(std::rethrow_exception(client_handler.error), SocketClosedPrematurelyError);
}
