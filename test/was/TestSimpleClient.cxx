// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

/*
 * Regression tests for the #Was::SimpleClient protocol state machine.
 *
 * Like TestSimpleServerProtocol.cxx, these tests do not use the
 * matching #Was::SimpleServer; they speak the control protocol
 * directly.  That allows sending packet sequences which the server
 * implementation would never produce - and the peer of a WAS client is
 * the (untrusted) WAS application, so those are exactly the sequences
 * the client has to survive.
 *
 * Note that #RawPeer can send several packets in one write(): they then
 * arrive in one BufferedSocket read and are handled in one
 * OnBufferedData() call, i.e. without an OnWasControlDrained() call in
 * between.  Which packets share a batch is under the peer's control,
 * and it changes which states the client passes through.
 */

#include "was/async/SimpleClient.hxx"
#include "was/async/SimpleHandler.hxx"
#include "was/async/SimpleResponse.hxx"
#include "was/async/Socket.hxx"
#include "event/FineTimerEvent.hxx"
#include "event/Loop.hxx"
#include "net/SocketProtocolError.hxx"
#include "util/Cancellable.hxx"
#include "util/SpanCast.hxx"
#include "util/Unaligned.hxx"

#include <was/protocol.h>

#include <gtest/gtest.h>

#include <string>
#include <utility>
#include <vector>

namespace {

struct Packet {
	uint16_t command;
	std::string payload;
};

/**
 * The peer end of a #WasSocket - i.e. the WAS application - speaking
 * the raw control protocol.
 */
class RawPeer {
	WasSocket socket;

	std::string input_buffer, output_buffer;

public:
	explicit RawPeer(WasSocket &&_socket) noexcept
		:socket(std::move(_socket))
	{
		socket.control.SetNonBlocking();
		socket.input.SetNonBlocking();
		socket.output.SetNonBlocking();
	}

	~RawPeer() noexcept {
		EXPECT_TRUE(output_buffer.empty()) << "missing Flush()";
	}

	void CloseControl() noexcept {
		socket.control.Close();
	}

	/**
	 * Append a packet to the pending write buffer.
	 */
	void Queue(enum was_command cmd,
		   std::span<const std::byte> payload={}) noexcept {
		const struct was_header header{
			.length = static_cast<uint16_t>(payload.size()),
			.command = static_cast<uint16_t>(cmd),
		};

		output_buffer.append((const char *)&header, sizeof(header));
		output_buffer.append((const char *)payload.data(), payload.size());
	}

	void QueueStatus(HttpStatus status) noexcept {
		Queue(WAS_COMMAND_STATUS, std::as_bytes(std::span{&status, 1}));
	}

	void QueueU64(enum was_command cmd, uint64_t value) noexcept {
		Queue(cmd, std::as_bytes(std::span{&value, 1}));
	}

	/**
	 * Write all queued packets in one write(), so the client
	 * handles them in a single OnBufferedData() call.
	 */
	void Flush() noexcept {
		if (output_buffer.empty())
			return;

		EXPECT_EQ(socket.control.Send(AsBytes(std::string_view{output_buffer})),
			  static_cast<ssize_t>(output_buffer.size()));
		output_buffer.clear();
	}

	void Send(enum was_command cmd,
		  std::span<const std::byte> payload={}) noexcept {
		Queue(cmd, payload);
		Flush();
	}

	void SendU64(enum was_command cmd, uint64_t value) noexcept {
		QueueU64(cmd, value);
		Flush();
	}

	/**
	 * Write to the pipe which the client reads its response body
	 * from.
	 */
	void WriteBody(std::string_view src) noexcept {
		EXPECT_EQ(socket.output.Write(AsBytes(src)),
			  static_cast<ssize_t>(src.size()));
	}

	/**
	 * Receive and parse all control packets which have arrived so
	 * far.
	 */
	std::vector<Packet> Receive() noexcept {
		while (true) {
			std::byte buffer[4096];
			auto nbytes = socket.control.Receive(buffer);
			if (nbytes <= 0)
				break;

			input_buffer.append((const char *)buffer, nbytes);
		}

		std::vector<Packet> result;

		std::size_t pos = 0;
		while (input_buffer.size() - pos >= sizeof(struct was_header)) {
			const auto header = LoadUnaligned<struct was_header>(input_buffer.data() + pos);
			if (input_buffer.size() - pos - sizeof(header) < header.length)
				break;

			result.emplace_back(header.command,
					    input_buffer.substr(pos + sizeof(header),
								header.length));
			pos += sizeof(header) + header.length;
		}

		input_buffer.erase(0, pos);
		return result;
	}
};

[[gnu::pure]]
static const Packet *
FindPacket(const std::vector<Packet> &packets, enum was_command cmd) noexcept
{
	for (const auto &i : packets)
		if (i.command == static_cast<uint16_t>(cmd))
			return &i;

	return nullptr;
}

/**
 * Owns a heap-allocated #Was::SimpleClient and deletes it from the
 * handler methods, mimicking what a consumer does when the connection
 * fails.
 */
class TestClient final : Was::SimpleClientHandler {
public:
	EventLoop event_loop;

private:
	FineTimerEvent break_timer;

	Was::SimpleClient *client;

public:
	std::exception_ptr error;

	explicit TestClient(WasSocket &&socket) noexcept
		:break_timer(event_loop, BIND_THIS_METHOD(OnBreakTimer)),
		 client(new Was::SimpleClient(event_loop, std::move(socket),
					      *this)) {}

	~TestClient() noexcept {
		delete client;
	}

	bool IsAlive() const noexcept {
		return client != nullptr;
	}

	Was::SimpleClient &Get() noexcept {
		return *client;
	}

	/**
	 * Run the #EventLoop until nothing happens for a short while.
	 */
	void Run(Event::Duration timeout=std::chrono::milliseconds{20}) noexcept {
		break_timer.Schedule(timeout);
		event_loop.Run();
		break_timer.Cancel();
	}

private:
	void OnBreakTimer() noexcept {
		event_loop.Break();
	}

	/* virtual methods from Was::SimpleClientHandler */
	void OnWasError(std::exception_ptr _error) noexcept override {
		error = std::move(_error);
		delete std::exchange(client, nullptr);
	}
};

class RecordingResponseHandler final : public Was::SimpleResponseHandler {
public:
	unsigned n_responses = 0, n_errors = 0;

	Was::SimpleResponse response;
	std::exception_ptr error;

	/* virtual methods from Was::SimpleResponseHandler */
	void OnWasResponse(Was::SimpleResponse &&_response) noexcept override {
		++n_responses;
		response = std::move(_response);
	}

	void OnWasError(std::exception_ptr _error) noexcept override {
		++n_errors;
		error = std::move(_error);
	}
};

static WasSocket
MakeNonBlocking(WasSocket &&socket) noexcept
{
	socket.input.SetNonBlocking();
	socket.output.SetNonBlocking();
	return std::move(socket);
}

/**
 * Bundles a #TestClient with its #RawPeer.
 */
struct Fixture {
	RawPeer peer;
	TestClient client;

	explicit Fixture(std::pair<WasSocket, WasSocket> pair=WasSocket::CreatePair())
		:peer(std::move(pair.second)),
		 client(MakeNonBlocking(std::move(pair.first))) {}

	/**
	 * Send a request and wait until the peer has received it.
	 */
	void SendRequest(RecordingResponseHandler &response_handler,
			 CancellablePointer &cancel_ptr,
			 const char *uri) noexcept {
		ASSERT_TRUE(client.IsAlive());
		ASSERT_TRUE(client.Get().SendRequest(Was::SimpleRequest{
					.uri = uri,
				}, response_handler, cancel_ptr));

		client.Run();

		EXPECT_TRUE(FindPacket(peer.Receive(), WAS_COMMAND_REQUEST));
	}
};

} // anonymous namespace

/**
 * A PREMATURE packet aborts the response body, but must leave the
 * connection usable - allowing the pipe to be flushed and reused is the
 * whole point of PREMATURE.
 *
 * DATA and PREMATURE are sent in one write(), so the client enters
 * State::BODY and leaves it again within a single OnBufferedData()
 * call.  SimpleInput::Premature() destroys the input buffer, so the
 * OnWasControlDrained() which follows the batch used to call
 * SimpleInput::CheckComplete() on it - guarded only by an assert(),
 * i.e. a null dereference in release builds.
 */
TEST(WasSimpleClient, PrematureDuringResponseBody)
{
	[[maybe_unused]] const ScopeInitDefaultFifoBuffer init;

	Fixture f;

	RecordingResponseHandler rh;
	CancellablePointer cancel_ptr;
	f.SendRequest(rh, cancel_ptr, "/foo");

	/* announce a response body and abort it right away, all in
	   one write() */
	f.peer.QueueStatus(HttpStatus::OK);
	f.peer.Queue(WAS_COMMAND_DATA);
	f.peer.QueueU64(WAS_COMMAND_PREMATURE, 0);
	f.peer.Flush();
	f.client.Run();

	/* the aborted response is reported as an error ... */
	EXPECT_EQ(rh.n_responses, 0u);
	EXPECT_EQ(rh.n_errors, 1u);

	/* ... but the connection must stay usable */
	EXPECT_FALSE(f.client.error);
	ASSERT_TRUE(f.client.IsAlive());

	RecordingResponseHandler rh2;
	CancellablePointer cancel_ptr2;
	f.SendRequest(rh2, cancel_ptr2, "/bar");

	f.peer.QueueStatus(HttpStatus::NOT_FOUND);
	f.peer.Queue(WAS_COMMAND_NO_DATA);
	f.peer.Flush();
	f.client.Run();

	EXPECT_EQ(rh2.n_errors, 0u);
	ASSERT_EQ(rh2.n_responses, 1u);
	EXPECT_EQ(rh2.response.status, HttpStatus::NOT_FOUND);
}

/**
 * The same, but the peer has announced a length and written part of the
 * body, so SimpleInput::Premature() has to flush those bytes out of the
 * pipe.
 */
TEST(WasSimpleClient, PrematureAfterPartialResponseBody)
{
	[[maybe_unused]] const ScopeInitDefaultFifoBuffer init;

	Fixture f;

	RecordingResponseHandler rh;
	CancellablePointer cancel_ptr;
	f.SendRequest(rh, cancel_ptr, "/foo");

	/* the body bytes are in the pipe before the control packets
	   announcing them */
	f.peer.WriteBody("hello");

	f.peer.QueueStatus(HttpStatus::OK);
	f.peer.Queue(WAS_COMMAND_DATA);
	f.peer.QueueU64(WAS_COMMAND_LENGTH, 16);
	f.peer.QueueU64(WAS_COMMAND_PREMATURE, 5);
	f.peer.Flush();
	f.client.Run();

	EXPECT_EQ(rh.n_responses, 0u);
	EXPECT_EQ(rh.n_errors, 1u);

	EXPECT_FALSE(f.client.error);
	ASSERT_TRUE(f.client.IsAlive());

	/* the pipe has been resynchronized, so the next request works */
	RecordingResponseHandler rh2;
	CancellablePointer cancel_ptr2;
	f.SendRequest(rh2, cancel_ptr2, "/bar");

	f.peer.QueueStatus(HttpStatus::OK);
	f.peer.Queue(WAS_COMMAND_NO_DATA);
	f.peer.Flush();
	f.client.Run();

	EXPECT_EQ(rh2.n_errors, 0u);
	EXPECT_EQ(rh2.n_responses, 1u);
}

/**
 * The PREMATURE which answers our STOP must clear the "stopping" flag;
 * a later unsolicited PREMATURE is then a protocol error.
 *
 * The flag used to be set by "if (stopping) stopping = true", a no-op,
 * so once Cancel() had set it, every later PREMATURE was accepted in
 * any state.
 */
TEST(WasSimpleClient, StoppingClearedByPremature)
{
	[[maybe_unused]] const ScopeInitDefaultFifoBuffer init;

	Fixture f;

	RecordingResponseHandler rh;
	CancellablePointer cancel_ptr;
	f.SendRequest(rh, cancel_ptr, "/foo");

	/* cancel before the peer has answered; this sends STOP and
	   expects a PREMATURE in reply */
	cancel_ptr.Cancel();
	f.client.Run();

	ASSERT_TRUE(f.client.IsAlive());
	EXPECT_TRUE(f.client.Get().IsStopping());
	EXPECT_TRUE(FindPacket(f.peer.Receive(), WAS_COMMAND_STOP));

	f.peer.SendU64(WAS_COMMAND_PREMATURE, 0);
	f.client.Run();

	ASSERT_TRUE(f.client.IsAlive());
	EXPECT_FALSE(f.client.Get().IsStopping());

	/* a second, unsolicited PREMATURE is misplaced now */
	f.peer.SendU64(WAS_COMMAND_PREMATURE, 0);
	f.client.Run();

	EXPECT_FALSE(f.client.IsAlive());
	ASSERT_TRUE(f.client.error);
	EXPECT_THROW(std::rethrow_exception(f.client.error),
		     SocketProtocolError);
}

/**
 * Cancel() while the response body is still being received: the
 * response handler must not be invoked afterwards, and the connection
 * must stay usable.
 */
TEST(WasSimpleClient, CancelDuringResponseBody)
{
	[[maybe_unused]] const ScopeInitDefaultFifoBuffer init;

	Fixture f;

	RecordingResponseHandler rh;
	CancellablePointer cancel_ptr;
	f.SendRequest(rh, cancel_ptr, "/foo");

	/* announce 16 bytes but write only 5 - Was::Output sends
	   LENGTH before it writes the body */
	f.peer.WriteBody("hello");
	f.peer.QueueStatus(HttpStatus::OK);
	f.peer.Queue(WAS_COMMAND_DATA);
	f.peer.QueueU64(WAS_COMMAND_LENGTH, 16);
	f.peer.Flush();
	f.client.Run();

	EXPECT_EQ(rh.n_responses, 0u);
	ASSERT_TRUE(f.client.IsAlive());

	cancel_ptr.Cancel();
	f.client.Run();

	EXPECT_TRUE(FindPacket(f.peer.Receive(), WAS_COMMAND_STOP));

	/* the peer writes the rest of the body before it processes our
	   STOP, so the buffer completes after the cancellation */
	f.peer.WriteBody("...........");
	f.client.Run();

	/* the handler must not be invoked, in either direction */
	EXPECT_EQ(rh.n_responses, 0u);
	EXPECT_EQ(rh.n_errors, 0u);
	ASSERT_TRUE(f.client.IsAlive());

	/* the peer answers our STOP, resynchronizing the pipe */
	f.peer.SendU64(WAS_COMMAND_PREMATURE, 16);
	f.client.Run();

	EXPECT_EQ(rh.n_responses, 0u);
	EXPECT_EQ(rh.n_errors, 0u);
	ASSERT_TRUE(f.client.IsAlive());
	EXPECT_FALSE(f.client.Get().IsStopping());
}

/**
 * A response body is delivered only once it is complete.
 */
TEST(WasSimpleClient, ResponseBody)
{
	[[maybe_unused]] const ScopeInitDefaultFifoBuffer init;

	Fixture f;

	RecordingResponseHandler rh;
	CancellablePointer cancel_ptr;
	f.SendRequest(rh, cancel_ptr, "/foo");

	/* announce 5 bytes, but write only 2 of them */
	f.peer.QueueStatus(HttpStatus::OK);
	f.peer.Queue(WAS_COMMAND_DATA);
	f.peer.QueueU64(WAS_COMMAND_LENGTH, 5);
	f.peer.Flush();
	f.peer.WriteBody("he");
	f.client.Run();

	/* incomplete: nothing may be delivered yet */
	EXPECT_EQ(rh.n_responses, 0u);
	EXPECT_EQ(rh.n_errors, 0u);
	ASSERT_TRUE(f.client.IsAlive());

	f.peer.WriteBody("llo");
	f.client.Run();

	EXPECT_EQ(rh.n_errors, 0u);
	ASSERT_EQ(rh.n_responses, 1u);
	EXPECT_EQ(rh.response.status, HttpStatus::OK);
	ASSERT_TRUE(rh.response.body);
}

/**
 * A response body of length 0 is complete right away.
 */
TEST(WasSimpleClient, EmptyResponseBody)
{
	[[maybe_unused]] const ScopeInitDefaultFifoBuffer init;

	Fixture f;

	RecordingResponseHandler rh;
	CancellablePointer cancel_ptr;
	f.SendRequest(rh, cancel_ptr, "/foo");

	f.peer.QueueStatus(HttpStatus::OK);
	f.peer.Queue(WAS_COMMAND_DATA);
	f.peer.QueueU64(WAS_COMMAND_LENGTH, 0);
	f.peer.Flush();
	f.client.Run();

	EXPECT_EQ(rh.n_errors, 0u);
	ASSERT_EQ(rh.n_responses, 1u);
	ASSERT_TRUE(rh.response.body);
}

/**
 * Cancel() while the response body is incomplete and stays incomplete:
 * the peer's PREMATURE reply reports how much it wrote, and that many
 * bytes are flushed out of the pipe.
 */
TEST(WasSimpleClient, CancelIncompleteResponseBody)
{
	[[maybe_unused]] const ScopeInitDefaultFifoBuffer init;

	Fixture f;

	RecordingResponseHandler rh;
	CancellablePointer cancel_ptr;
	f.SendRequest(rh, cancel_ptr, "/foo");

	f.peer.QueueStatus(HttpStatus::OK);
	f.peer.Queue(WAS_COMMAND_DATA);
	f.peer.QueueU64(WAS_COMMAND_LENGTH, 16);
	f.peer.Flush();
	f.peer.WriteBody("hello");
	f.client.Run();

	cancel_ptr.Cancel();
	f.client.Run();

	EXPECT_TRUE(FindPacket(f.peer.Receive(), WAS_COMMAND_STOP));

	/* the peer gives up after the 5 bytes it had written */
	f.peer.SendU64(WAS_COMMAND_PREMATURE, 5);
	f.client.Run();

	EXPECT_EQ(rh.n_responses, 0u);
	EXPECT_EQ(rh.n_errors, 0u);
	ASSERT_TRUE(f.client.IsAlive());
	EXPECT_FALSE(f.client.Get().IsStopping());

	/* the connection is usable again */
	RecordingResponseHandler rh2;
	CancellablePointer cancel_ptr2;
	f.SendRequest(rh2, cancel_ptr2, "/bar");

	f.peer.QueueStatus(HttpStatus::OK);
	f.peer.Queue(WAS_COMMAND_NO_DATA);
	f.peer.Flush();
	f.client.Run();

	EXPECT_EQ(rh2.n_errors, 0u);
	EXPECT_EQ(rh2.n_responses, 1u);
}

/**
 * A protocol error while a request is pending must fail that request
 * and close the connection.
 *
 * AbortError() used to call Close() without leaving the request state,
 * and Close() requires State::IDLE.
 */
TEST(WasSimpleClient, ProtocolErrorDuringRequest)
{
	[[maybe_unused]] const ScopeInitDefaultFifoBuffer init;

	Fixture f;

	RecordingResponseHandler rh;
	CancellablePointer cancel_ptr;
	f.SendRequest(rh, cancel_ptr, "/foo");

	/* the peer must not send request packets */
	f.peer.Send(WAS_COMMAND_REQUEST);
	f.client.Run();

	EXPECT_EQ(rh.n_responses, 0u);
	EXPECT_EQ(rh.n_errors, 1u);

	EXPECT_FALSE(f.client.IsAlive());
	ASSERT_TRUE(f.client.error);
	EXPECT_THROW(std::rethrow_exception(f.client.error),
		     SocketProtocolError);
}

/**
 * The peer closes the control socket while a request is pending; same
 * Close() precondition as above, reached via OnWasControlHangup().
 */
TEST(WasSimpleClient, ControlClosedDuringRequest)
{
	[[maybe_unused]] const ScopeInitDefaultFifoBuffer init;

	Fixture f;

	RecordingResponseHandler rh;
	CancellablePointer cancel_ptr;
	f.SendRequest(rh, cancel_ptr, "/foo");

	f.peer.CloseControl();
	f.client.Run();

	EXPECT_EQ(rh.n_responses, 0u);
	EXPECT_EQ(rh.n_errors, 1u);

	EXPECT_FALSE(f.client.IsAlive());
	ASSERT_TRUE(f.client.error);
	EXPECT_THROW(std::rethrow_exception(f.client.error),
		     SocketClosedPrematurelyError);
}

/**
 * A new request cannot be submitted while the previous one is still
 * being canceled: the peer's PREMATURE reply to our STOP is
 * outstanding, and the body pipe still holds the old response body.
 *
 * This is what makes "the input is active but nobody is waiting for it"
 * an invariant in OnWasInput().
 */
TEST(WasSimpleClient, NoReuseWhileStopping)
{
	[[maybe_unused]] const ScopeInitDefaultFifoBuffer init;

	Fixture f;

	RecordingResponseHandler rh;
	CancellablePointer cancel_ptr;
	f.SendRequest(rh, cancel_ptr, "/foo");

	f.peer.QueueStatus(HttpStatus::OK);
	f.peer.Queue(WAS_COMMAND_DATA);
	f.peer.QueueU64(WAS_COMMAND_LENGTH, 16);
	f.peer.Flush();
	f.peer.WriteBody("hello");
	f.client.Run();

	cancel_ptr.Cancel();
	f.client.Run();

	ASSERT_TRUE(f.client.IsAlive());
	ASSERT_TRUE(f.client.Get().IsStopping());

	RecordingResponseHandler rh2;
	CancellablePointer cancel_ptr2;
	EXPECT_FALSE(f.client.Get().SendRequest(Was::SimpleRequest{
				.uri = "/bar",
			}, rh2, cancel_ptr2));

	/* the rest of the old body arrives and is dropped */
	f.peer.WriteBody("...........");
	f.client.Run();

	EXPECT_EQ(rh.n_responses, 0u);
	EXPECT_EQ(rh.n_errors, 0u);
	EXPECT_EQ(rh2.n_responses, 0u);
	EXPECT_EQ(rh2.n_errors, 0u);
	ASSERT_TRUE(f.client.IsAlive());

	/* once the handshake is done, the connection is usable again */
	f.peer.SendU64(WAS_COMMAND_PREMATURE, 16);
	f.client.Run();

	EXPECT_FALSE(f.client.Get().IsStopping());
	f.SendRequest(rh2, cancel_ptr2, "/bar");

	f.peer.QueueStatus(HttpStatus::OK);
	f.peer.Queue(WAS_COMMAND_NO_DATA);
	f.peer.Flush();
	f.client.Run();

	EXPECT_EQ(rh2.n_errors, 0u);
	EXPECT_EQ(rh2.n_responses, 1u);
}
