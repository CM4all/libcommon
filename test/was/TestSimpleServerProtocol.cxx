// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

/*
 * Regression tests for the #Was::SimpleServer protocol state machine.
 *
 * Unlike TestSimpleServer.cxx, these tests do not use #Was::SimpleClient;
 * they speak the control protocol directly.  That allows sending
 * malformed and misordered packets which a well-behaved peer would never
 * send - which is exactly the class of input that the server has to
 * reject safely.
 *
 * The #Was::SimpleServerHandler used here deletes the #Was::SimpleServer
 * from OnWasError(), just like ConnectionList in SimpleRun.cxx does; that
 * is what makes a packet handler which keeps going after
 * AbortProtocolError() observable as a use-after-free instead of a
 * harmless no-op.
 */

#include "was/async/SimpleHandler.hxx"
#include "was/async/SimpleResponse.hxx"
#include "was/async/SimpleServer.hxx"
#include "was/async/Socket.hxx"
#include "was/async/StringOutputProducer.hxx"
#include "event/FineTimerEvent.hxx"
#include "event/Loop.hxx"
#include "net/SocketProtocolError.hxx"
#include "util/Cancellable.hxx"
#include "util/DisposableBuffer.hxx"
#include "util/SpanCast.hxx"
#include "util/Unaligned.hxx"

#include <was/protocol.h>

#include <gtest/gtest.h>

#include <sys/socket.h>

#include <string>
#include <utility>
#include <vector>

namespace {

/* the size of Was::Control's output buffer (DefaultFifoBuffer) */
static constexpr std::size_t CONTROL_BUFFER_SIZE = 8192;

struct Packet {
	uint16_t command;
	std::string payload;
};

/**
 * The peer end of a #WasSocket, speaking the raw control protocol.
 */
class RawPeer {
	WasSocket socket;

	std::string input_buffer;

public:
	explicit RawPeer(WasSocket &&_socket) noexcept
		:socket(std::move(_socket))
	{
		socket.control.SetNonBlocking();
		socket.input.SetNonBlocking();
		socket.output.SetNonBlocking();
	}

	void CloseControl() noexcept {
		socket.control.Close();
	}

	void SendRaw(uint16_t cmd, std::span<const std::byte> payload) noexcept {
		std::string buffer;
		const struct was_header header{
			.length = static_cast<uint16_t>(payload.size()),
			.command = cmd,
		};

		buffer.append((const char *)&header, sizeof(header));
		buffer.append((const char *)payload.data(), payload.size());

		EXPECT_EQ(socket.control.Send(AsBytes(std::string_view{buffer})),
			  static_cast<ssize_t>(buffer.size()));
	}

	void Send(enum was_command cmd,
		  std::span<const std::byte> payload={}) noexcept {
		SendRaw(static_cast<uint16_t>(cmd), payload);
	}

	void SendString(enum was_command cmd, std::string_view payload) noexcept {
		Send(cmd, AsBytes(payload));
	}

	void SendU32(enum was_command cmd, uint32_t value) noexcept {
		Send(cmd, std::as_bytes(std::span{&value, 1}));
	}

	void SendU64(enum was_command cmd, uint64_t value) noexcept {
		Send(cmd, std::as_bytes(std::span{&value, 1}));
	}

	/**
	 * Write to the pipe which the server reads its request body from.
	 */
	void WriteBody(std::string_view src) noexcept {
		EXPECT_EQ(socket.output.Write(AsBytes(src)),
			  static_cast<ssize_t>(src.size()));
	}

	/**
	 * Receive and parse all control packets which have arrived so far.
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
 * Owns a heap-allocated #Was::SimpleServer and deletes it from the
 * handler methods, mimicking ConnectionList in SimpleRun.cxx.
 */
class TestServer final : Was::SimpleServerHandler {
public:
	EventLoop event_loop;

private:
	FineTimerEvent break_timer;

	Was::SimpleServer *server;

public:
	std::exception_ptr error;
	bool closed = false;

	TestServer(WasSocket &&socket,
		   Was::SimpleRequestHandler &request_handler) noexcept
		:break_timer(event_loop, BIND_THIS_METHOD(OnBreakTimer)),
		 server(new Was::SimpleServer(event_loop, std::move(socket),
					      *this, request_handler)) {}

	~TestServer() noexcept {
		delete server;
	}

	bool IsAlive() const noexcept {
		return server != nullptr;
	}

	Was::SimpleServer &Get() noexcept {
		return *server;
	}

	void Destroy() noexcept {
		delete std::exchange(server, nullptr);
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

	/* virtual methods from Was::SimpleServerHandler */
	void OnWasError(Was::SimpleServer &,
			std::exception_ptr _error) noexcept override {
		error = std::move(_error);
		Destroy();
	}

	void OnWasClosed(Was::SimpleServer &) noexcept override {
		closed = true;
		Destroy();
	}
};

class RecordingRequestHandler final
	: public Was::SimpleRequestHandler, Cancellable
{
public:
	enum class Mode {
		/**
		 * Send the configured response right away.
		 */
		RESPOND,

		/**
		 * Register #cancel_ptr and do nothing.
		 */
		DEFER,
	} mode;

	/* the response sent in Mode::RESPOND */
	HttpStatus status = HttpStatus::OK;
	std::multimap<std::string, std::string, std::less<>> response_headers;
	std::string response_body;
	bool have_response_body = false;

	/* what was received */
	unsigned n_requests = 0;
	bool canceled = false;
	HttpMethod method{};
	std::string uri, body;
	bool had_body = false;

	explicit RecordingRequestHandler(Mode _mode) noexcept
		:mode(_mode) {}

	/* virtual methods from Was::SimpleRequestHandler */
	bool OnRequest(Was::SimpleServer &server, Was::SimpleRequest &&request,
		       CancellablePointer &cancel_ptr) noexcept override {
		++n_requests;
		method = request.method;
		uri = request.uri;
		had_body = !!request.body;
		body = had_body
			? std::string{std::string_view{request.body}}
			: std::string{};

		switch (mode) {
		case Mode::RESPOND:
			return server.SendResponse({
				.status = status,
				.headers = response_headers,
				.body = have_response_body
					? std::make_unique<Was::StringOutputProducer>(std::string{response_body})
					: nullptr,
			});

		case Mode::DEFER:
			cancel_ptr = *this;
			return true;
		}

		// unreachable
		std::terminate();
	}

private:
	/* virtual methods from class Cancellable */
	void Cancel() noexcept override {
		canceled = true;
	}
};

/**
 * WasSocket::CreatePair() returns blocking pipes; SimpleInput::TryRead()
 * would block the whole #EventLoop on them.  A real container passes
 * non-blocking pipes, so make them non-blocking here as well.
 */
static WasSocket
MakeNonBlocking(WasSocket &&socket) noexcept
{
	socket.input.SetNonBlocking();
	socket.output.SetNonBlocking();
	return std::move(socket);
}

/**
 * Bundles a #TestServer with its #RawPeer.
 */
struct Fixture {
	RecordingRequestHandler request_handler;
	RawPeer peer;
	TestServer server;

	explicit Fixture(RecordingRequestHandler::Mode mode,
			 std::pair<WasSocket, WasSocket> pair=WasSocket::CreatePair())
		:request_handler(mode),
		 peer(std::move(pair.first)),
		 server(MakeNonBlocking(std::move(pair.second)),
			request_handler) {}
};

static void
ExpectProtocolError(const TestServer &server)
{
	EXPECT_TRUE(server.error) << "expected the server to reject the packet";
	EXPECT_FALSE(server.IsAlive());

	if (server.error) {
		EXPECT_THROW(std::rethrow_exception(server.error),
			     SocketProtocolError);
	}
}

/**
 * Send a bare metadata packet on an idle connection (no REQUEST packet
 * was sent), which must be rejected without touching the (unset)
 * #SimpleRequest.
 *
 * These handlers used to call AbortProtocolError() without returning,
 * and then went on to dereference the std::optional which
 * AbortProtocolError() had just reset.
 */
static void
TestMisplacedMetadata(enum was_command cmd)
{
	[[maybe_unused]] const ScopeInitDefaultFifoBuffer init;

	Fixture f{RecordingRequestHandler::Mode::RESPOND};

	f.peer.SendString(cmd, "value");
	f.server.Run();

	ExpectProtocolError(f.server);
	EXPECT_EQ(f.request_handler.n_requests, 0u);
}

} // anonymous namespace

/**
 * A SCRIPT_NAME packet without a preceding REQUEST must be rejected.
 */
TEST(WasServerProtocol, MisplacedScriptName)
{
	TestMisplacedMetadata(WAS_COMMAND_SCRIPT_NAME);
}

/**
 * A PATH_INFO packet without a preceding REQUEST must be rejected.
 */
TEST(WasServerProtocol, MisplacedPathInfo)
{
	TestMisplacedMetadata(WAS_COMMAND_PATH_INFO);
}

/**
 * A QUERY_STRING packet without a preceding REQUEST must be rejected.
 */
TEST(WasServerProtocol, MisplacedQueryString)
{
	TestMisplacedMetadata(WAS_COMMAND_QUERY_STRING);
}

/**
 * A REMOTE_HOST packet without a preceding REQUEST must be rejected.
 */
TEST(WasServerProtocol, MisplacedRemoteHost)
{
	TestMisplacedMetadata(WAS_COMMAND_REMOTE_HOST);
}

/**
 * A DOCUMENT_ROOT packet without a preceding REQUEST must be rejected.
 */
TEST(WasServerProtocol, MisplacedDocumentRoot)
{
	TestMisplacedMetadata(WAS_COMMAND_DOCUMENT_ROOT);
}

/**
 * Metadata packets are illegal once the request metadata is complete,
 * even though a #SimpleRequest exists at that point.
 */
TEST(WasServerProtocol, ScriptNameAfterNoData)
{
	[[maybe_unused]] const ScopeInitDefaultFifoBuffer init;

	Fixture f{RecordingRequestHandler::Mode::DEFER};

	f.peer.Send(WAS_COMMAND_REQUEST);
	f.peer.SendString(WAS_COMMAND_URI, "/foo");
	f.peer.Send(WAS_COMMAND_NO_DATA);
	f.server.Run();

	ASSERT_EQ(f.request_handler.n_requests, 1u);
	ASSERT_TRUE(f.server.IsAlive());

	f.peer.SendString(WAS_COMMAND_SCRIPT_NAME, "/foo");
	f.server.Run();

	ExpectProtocolError(f.server);
}

/**
 * A TLS packet without a preceding REQUEST must be rejected; this
 * handler used to have no state check at all.
 */
TEST(WasServerProtocol, MisplacedTls)
{
	[[maybe_unused]] const ScopeInitDefaultFifoBuffer init;

	Fixture f{RecordingRequestHandler::Mode::RESPOND};

	f.peer.Send(WAS_COMMAND_TLS);
	f.server.Run();

	ExpectProtocolError(f.server);
	EXPECT_EQ(f.request_handler.n_requests, 0u);
}

/**
 * A PREMATURE packet cancels the request whose body it aborts, but must
 * leave the connection usable - allowing the pipe to be flushed and
 * reused is the whole point of PREMATURE.
 *
 * This used to leave request.state==BODY behind, so that the next
 * OnWasControlDrained() called SimpleInput::CheckComplete() on the
 * buffer PREMATURE had just destroyed.
 */
TEST(WasServerProtocol, PrematureDuringRequestBody)
{
	[[maybe_unused]] const ScopeInitDefaultFifoBuffer init;

	Fixture f{RecordingRequestHandler::Mode::DEFER};

	f.peer.Send(WAS_COMMAND_REQUEST);
	f.peer.SendString(WAS_COMMAND_URI, "/foo");
	f.peer.Send(WAS_COMMAND_DATA);
	f.server.Run();

	ASSERT_TRUE(f.server.IsAlive());
	ASSERT_EQ(f.request_handler.n_requests, 0u);

	/* a separate write, so that PREMATURE is handled in its own
	   OnBufferedData() call and is followed by a drain */
	f.peer.SendU64(WAS_COMMAND_PREMATURE, 0);
	f.server.Run();

	/* the aborted request must never reach the request handler ... */
	EXPECT_EQ(f.request_handler.n_requests, 0u);

	/* ... but the connection must stay usable */
	EXPECT_FALSE(f.server.error);
	ASSERT_TRUE(f.server.IsAlive());

	f.request_handler.mode = RecordingRequestHandler::Mode::RESPOND;
	f.peer.Send(WAS_COMMAND_REQUEST);
	f.peer.SendString(WAS_COMMAND_URI, "/bar");
	f.peer.Send(WAS_COMMAND_NO_DATA);
	f.server.Run();

	EXPECT_EQ(f.request_handler.n_requests, 1u);
	EXPECT_EQ(f.request_handler.uri, "/bar");
	EXPECT_TRUE(FindPacket(f.peer.Receive(), WAS_COMMAND_STATUS));
}

/**
 * The same, but part of the request body has already been consumed when
 * PREMATURE arrives.
 */
TEST(WasServerProtocol, PrematureAfterPartialRequestBody)
{
	[[maybe_unused]] const ScopeInitDefaultFifoBuffer init;

	Fixture f{RecordingRequestHandler::Mode::DEFER};

	f.peer.Send(WAS_COMMAND_REQUEST);
	f.peer.SendString(WAS_COMMAND_URI, "/foo");
	f.peer.Send(WAS_COMMAND_DATA);
	f.peer.SendU64(WAS_COMMAND_LENGTH, 4);
	f.peer.WriteBody("ab");
	f.server.Run();

	ASSERT_TRUE(f.server.IsAlive());
	ASSERT_EQ(f.request_handler.n_requests, 0u);

	f.peer.SendU64(WAS_COMMAND_PREMATURE, 2);
	f.server.Run();

	EXPECT_EQ(f.request_handler.n_requests, 0u);
	EXPECT_FALSE(f.server.error);
	ASSERT_TRUE(f.server.IsAlive());

	/* the next request must get its own body, not the remains of the
	   aborted one */
	f.request_handler.mode = RecordingRequestHandler::Mode::RESPOND;
	f.peer.Send(WAS_COMMAND_REQUEST);
	f.peer.SendString(WAS_COMMAND_URI, "/bar");
	f.peer.Send(WAS_COMMAND_DATA);
	f.peer.SendU64(WAS_COMMAND_LENGTH, 3);
	f.peer.WriteBody("xyz");
	f.server.Run();

	EXPECT_EQ(f.request_handler.n_requests, 1u);
	EXPECT_EQ(f.request_handler.uri, "/bar");
	EXPECT_EQ(f.request_handler.body, "xyz");
}

/**
 * A STOP packet which arrives while the request body is still sitting in
 * the pipe must discard that body, and must not deliver it to the
 * request handler.
 *
 * SimpleClient::Cancel() sends PREMATURE before STOP only while its
 * request body output is still active; once it has written the whole
 * body to the pipe, it sends a bare STOP.  Everything is therefore sent
 * before the first Run(), so that STOP is handled while the body is
 * still unread.
 *
 * STOP used to leave #SimpleInput active, breaking the invariant
 * "input.IsActive() implies request.state == BODY".
 */
TEST(WasServerProtocol, StopDuringRequestBody)
{
	[[maybe_unused]] const ScopeInitDefaultFifoBuffer init;

	Fixture f{RecordingRequestHandler::Mode::DEFER};

	f.peer.Send(WAS_COMMAND_REQUEST);
	f.peer.SendString(WAS_COMMAND_URI, "/foo");
	f.peer.Send(WAS_COMMAND_DATA);
	f.peer.SendU64(WAS_COMMAND_LENGTH, 4);
	f.peer.WriteBody("abcd");
	f.peer.Send(WAS_COMMAND_STOP);
	f.server.Run();

	EXPECT_EQ(f.request_handler.n_requests, 0u);
	EXPECT_TRUE(FindPacket(f.peer.Receive(), WAS_COMMAND_PREMATURE));
	EXPECT_FALSE(f.server.error);
	EXPECT_TRUE(f.server.IsAlive());
}

/**
 * After a request was aborted with STOP, the leftover bytes of its body
 * must not leak into the next request.
 */
TEST(WasServerProtocol, StopDuringRequestBodyThenReuse)
{
	[[maybe_unused]] const ScopeInitDefaultFifoBuffer init;

	Fixture f{RecordingRequestHandler::Mode::DEFER};

	f.peer.Send(WAS_COMMAND_REQUEST);
	f.peer.SendString(WAS_COMMAND_URI, "/foo");
	f.peer.Send(WAS_COMMAND_DATA);
	f.peer.SendU64(WAS_COMMAND_LENGTH, 4);
	f.peer.WriteBody("abcd");
	f.peer.Send(WAS_COMMAND_STOP);
	f.server.Run();

	ASSERT_TRUE(f.server.IsAlive());

	/* the next request must start from a clean slate */
	f.request_handler.mode = RecordingRequestHandler::Mode::RESPOND;
	f.peer.Send(WAS_COMMAND_REQUEST);
	f.peer.SendString(WAS_COMMAND_URI, "/bar");
	f.peer.Send(WAS_COMMAND_DATA);
	f.peer.SendU64(WAS_COMMAND_LENGTH, 3);
	f.peer.WriteBody("xyz");
	f.server.Run();

	EXPECT_EQ(f.request_handler.n_requests, 1u);
	EXPECT_EQ(f.request_handler.uri, "/bar");
	EXPECT_EQ(f.request_handler.body, "xyz");
}

/**
 * The same, but the request body has already been partially consumed
 * when STOP arrives.
 */
TEST(WasServerProtocol, StopAfterPartiallyConsumedRequestBody)
{
	[[maybe_unused]] const ScopeInitDefaultFifoBuffer init;

	Fixture f{RecordingRequestHandler::Mode::DEFER};

	f.peer.Send(WAS_COMMAND_REQUEST);
	f.peer.SendString(WAS_COMMAND_URI, "/foo");
	f.peer.Send(WAS_COMMAND_DATA);
	f.peer.SendU64(WAS_COMMAND_LENGTH, 4);
	f.peer.WriteBody("ab");
	f.server.Run();

	/* the peer writes the rest and aborts right away; whether the
	   body completes before STOP is handled depends on whether the
	   pipe or the control socket is serviced first, and both
	   interleavings have to leave the pipe synchronized */
	f.peer.WriteBody("cd");
	f.peer.Send(WAS_COMMAND_STOP);
	f.server.Run();

	EXPECT_FALSE(f.server.error);
	ASSERT_TRUE(f.server.IsAlive());

	const unsigned n_before = f.request_handler.n_requests;

	f.request_handler.mode = RecordingRequestHandler::Mode::RESPOND;
	f.peer.Send(WAS_COMMAND_REQUEST);
	f.peer.SendString(WAS_COMMAND_URI, "/bar");
	f.peer.Send(WAS_COMMAND_DATA);
	f.peer.SendU64(WAS_COMMAND_LENGTH, 3);
	f.peer.WriteBody("xyz");
	f.server.Run();

	EXPECT_EQ(f.request_handler.n_requests, n_before + 1);
	EXPECT_EQ(f.request_handler.uri, "/bar");
	EXPECT_EQ(f.request_handler.body, "xyz");
}

/**
 * A peer which stops sending its request body without announcing a
 * LENGTH leaves the pipe unsynchronized; it has to send PREMATURE
 * instead (which is what SimpleClient::Cancel() does).  The connection
 * cannot be reused and must fail cleanly.
 */
TEST(WasServerProtocol, StopWithoutLength)
{
	[[maybe_unused]] const ScopeInitDefaultFifoBuffer init;

	Fixture f{RecordingRequestHandler::Mode::DEFER};

	f.peer.Send(WAS_COMMAND_REQUEST);
	f.peer.SendString(WAS_COMMAND_URI, "/foo");
	f.peer.Send(WAS_COMMAND_DATA);
	f.peer.Send(WAS_COMMAND_STOP);
	f.server.Run();

	ExpectProtocolError(f.server);
	EXPECT_EQ(f.request_handler.n_requests, 0u);
}

/**
 * Likewise, a peer which announces more than it actually wrote owes a
 * PREMATURE packet; STOP alone cannot resynchronize the pipe.
 */
TEST(WasServerProtocol, StopWithIncompleteRequestBody)
{
	[[maybe_unused]] const ScopeInitDefaultFifoBuffer init;

	Fixture f{RecordingRequestHandler::Mode::DEFER};

	f.peer.Send(WAS_COMMAND_REQUEST);
	f.peer.SendString(WAS_COMMAND_URI, "/foo");
	f.peer.Send(WAS_COMMAND_DATA);
	f.peer.SendU64(WAS_COMMAND_LENGTH, 4);
	f.peer.WriteBody("ab");
	f.server.Run();

	f.peer.Send(WAS_COMMAND_STOP);
	f.server.Run();

	ExpectProtocolError(f.server);
	EXPECT_EQ(f.request_handler.n_requests, 0u);
}

/**
 * A LENGTH smaller than the number of bytes already received must be
 * rejected.  Because Buffer::IsComplete() compares with ==, such a
 * length can never be reached, and the request used to hang forever
 * without any error.
 */
TEST(WasServerProtocol, LengthBelowFill)
{
	[[maybe_unused]] const ScopeInitDefaultFifoBuffer init;

	Fixture f{RecordingRequestHandler::Mode::RESPOND};

	f.peer.Send(WAS_COMMAND_REQUEST);
	f.peer.SendString(WAS_COMMAND_URI, "/foo");
	f.peer.Send(WAS_COMMAND_DATA);
	f.server.Run();

	f.peer.WriteBody("abcd");
	f.server.Run();

	f.peer.SendU64(WAS_COMMAND_LENGTH, 2);
	f.server.Run();

	ExpectProtocolError(f.server);
	EXPECT_EQ(f.request_handler.n_requests, 0u);
}

/**
 * A peer which writes more than it announced is violating the protocol
 * and has desynchronized the pipe beyond repair; the request body must
 * still be exactly what was announced, and the surplus must be left in
 * the pipe instead of being appended to the body.
 *
 * Reading was not clamped to the announced length, so the surplus ended
 * up in the buffer and the request never completed.
 */
TEST(WasServerProtocol, MoreDataThanAnnouncedLength)
{
	[[maybe_unused]] const ScopeInitDefaultFifoBuffer init;

	Fixture f{RecordingRequestHandler::Mode::RESPOND};

	f.peer.Send(WAS_COMMAND_REQUEST);
	f.peer.SendString(WAS_COMMAND_URI, "/foo");
	f.peer.Send(WAS_COMMAND_DATA);
	f.peer.SendU64(WAS_COMMAND_LENGTH, 2);
	f.peer.WriteBody("abcd");
	f.server.Run();

	EXPECT_EQ(f.request_handler.n_requests, 1u);
	EXPECT_EQ(f.request_handler.body, "ab");
}

/**
 * A LENGTH beyond Was::Buffer::max_size() must be rejected.
 */
TEST(WasServerProtocol, LengthTooLarge)
{
	[[maybe_unused]] const ScopeInitDefaultFifoBuffer init;

	Fixture f{RecordingRequestHandler::Mode::RESPOND};

	f.peer.Send(WAS_COMMAND_REQUEST);
	f.peer.SendString(WAS_COMMAND_URI, "/foo");
	f.peer.Send(WAS_COMMAND_DATA);
	f.peer.SendU64(WAS_COMMAND_LENGTH, 1024 * 1024);
	f.server.Run();

	ExpectProtocolError(f.server);
}

/**
 * A PREMATURE packet announcing more than the peer actually wrote must
 * be rejected.  The peer writes to the pipe before announcing, so those
 * bytes are necessarily readable by the time we see the packet; if they
 * are not, the peer is lying and the pipe can no longer be
 * resynchronized.
 */
TEST(WasServerProtocol, PrematureBeyondWrittenData)
{
	[[maybe_unused]] const ScopeInitDefaultFifoBuffer init;

	Fixture f{RecordingRequestHandler::Mode::DEFER};

	f.peer.Send(WAS_COMMAND_REQUEST);
	f.peer.SendString(WAS_COMMAND_URI, "/foo");
	f.peer.Send(WAS_COMMAND_DATA);
	f.server.Run();

	/* announce 4 bytes which were never written to the pipe */
	f.peer.SendU64(WAS_COMMAND_PREMATURE, 4);
	f.server.Run();

	ExpectProtocolError(f.server);
	EXPECT_EQ(f.request_handler.n_requests, 0u);
}

/**
 * Conversely, a PREMATURE packet announcing less than was already
 * received must be rejected as well.
 */
TEST(WasServerProtocol, PrematureBelowReceivedData)
{
	[[maybe_unused]] const ScopeInitDefaultFifoBuffer init;

	Fixture f{RecordingRequestHandler::Mode::DEFER};

	f.peer.Send(WAS_COMMAND_REQUEST);
	f.peer.SendString(WAS_COMMAND_URI, "/foo");
	f.peer.Send(WAS_COMMAND_DATA);
	f.peer.WriteBody("abcd");
	f.server.Run();

	f.peer.SendU64(WAS_COMMAND_PREMATURE, 2);
	f.server.Run();

	ExpectProtocolError(f.server);
	EXPECT_EQ(f.request_handler.n_requests, 0u);
}

/**
 * A control packet sent after a partial socket write must not be
 * rejected while the output buffer still has room.  Control::Start()
 * uses ForeignFifoBuffer::Write(), which only shifts when tail==size, so
 * it sees just the gap at the tail and gives up with "control output is
 * too large".
 */
TEST(WasServerProtocol, ControlOutputAfterPartialWrite)
{
	[[maybe_unused]] const ScopeInitDefaultFifoBuffer init;

	auto pair = WasSocket::CreatePair();

	/* make the control socket buffers small so that the response
	   below cannot be written in one go */
	pair.second.control.SetIntOption(SOL_SOCKET, SO_SNDBUF, 2048);
	pair.first.control.SetIntOption(SOL_SOCKET, SO_RCVBUF, 2048);

	Fixture f{RecordingRequestHandler::Mode::RESPOND, std::move(pair)};

	/* fill Control's output buffer to within less than the size of a
	   PREMATURE packet (4+8 bytes) of its end */
	static constexpr std::size_t TARGET = CONTROL_BUFFER_SIZE - 4;
	std::size_t total = 6 /* STATUS */ + 4 /* NO_DATA */;
	unsigned n = 0;
	while (total + 249 + 10 <= TARGET) {
		char name[8];
		std::snprintf(name, sizeof(name), "h%04u", n++);
		f.request_handler.response_headers.emplace(name,
							   std::string(239, 'x'));
		total += 4 + 5 + 1 + 239;
	}

	{
		char name[8];
		std::snprintf(name, sizeof(name), "h%04u", n++);
		const std::size_t value_size = TARGET - total - 10;
		f.request_handler.response_headers.emplace(name,
							   std::string(value_size, 'x'));
		total += 4 + 5 + 1 + value_size;
	}

	ASSERT_EQ(total, TARGET);

	f.peer.Send(WAS_COMMAND_REQUEST);
	f.peer.SendString(WAS_COMMAND_URI, "/foo");
	f.peer.Send(WAS_COMMAND_NO_DATA);
	f.server.Run();

	ASSERT_EQ(f.request_handler.n_requests, 1u);
	ASSERT_TRUE(f.server.IsAlive());

	/* the peer never read the response, so Control's output buffer
	   still holds the unwritten tail; this PREMATURE reply does not
	   fit after it, but it does fit after a Shift() */
	f.peer.Send(WAS_COMMAND_STOP);
	f.server.Run();

	if (f.server.error) {
		try {
			std::rethrow_exception(f.server.error);
		} catch (const std::exception &e) {
			ADD_FAILURE() << "the control output buffer was not "
					 "shifted before giving up: " << e.what();
		}
	}

	EXPECT_TRUE(f.server.IsAlive());
}

/**
 * Destructing the #Was::SimpleServer must cancel the pending request;
 * otherwise the request handler keeps running with a dangling reference
 * to it.
 */
TEST(WasServerProtocol, DestroyWithPendingRequest)
{
	[[maybe_unused]] const ScopeInitDefaultFifoBuffer init;

	Fixture f{RecordingRequestHandler::Mode::DEFER};

	f.peer.Send(WAS_COMMAND_REQUEST);
	f.peer.SendString(WAS_COMMAND_URI, "/foo");
	f.peer.Send(WAS_COMMAND_NO_DATA);
	f.server.Run();

	ASSERT_EQ(f.request_handler.n_requests, 1u);
	ASSERT_TRUE(f.server.IsAlive());

	f.server.Destroy();

	EXPECT_TRUE(f.request_handler.canceled)
		<< "destructing the server must cancel the pending request";
}

/**
 * A METHOD value which does not fit into #HttpMethod must be rejected.
 * The 32 bit payload is cast to the 8 bit underlying type before it is
 * validated, so the surplus bits are silently dropped.
 */
TEST(WasServerProtocol, MethodOutOfRange)
{
	[[maybe_unused]] const ScopeInitDefaultFifoBuffer init;

	Fixture f{RecordingRequestHandler::Mode::RESPOND};

	f.peer.Send(WAS_COMMAND_REQUEST);
	/* 0x100 | POST: truncates to POST */
	f.peer.SendU32(WAS_COMMAND_METHOD,
		       0x100 | static_cast<uint32_t>(HttpMethod::POST));
	f.peer.SendString(WAS_COMMAND_URI, "/foo");
	f.peer.Send(WAS_COMMAND_NO_DATA);
	f.server.Run();

	ExpectProtocolError(f.server);
	EXPECT_EQ(f.request_handler.n_requests, 0u);
}

/**
 * A repeated METHOD packet must be rejected.  The check compares the new
 * value with the stored one, so a packet which repeats the same method
 * slips through.
 */
TEST(WasServerProtocol, DuplicateMethod)
{
	[[maybe_unused]] const ScopeInitDefaultFifoBuffer init;

	Fixture f{RecordingRequestHandler::Mode::RESPOND};

	f.peer.Send(WAS_COMMAND_REQUEST);
	f.peer.SendU32(WAS_COMMAND_METHOD,
		       static_cast<uint32_t>(HttpMethod::POST));
	f.peer.SendU32(WAS_COMMAND_METHOD,
		       static_cast<uint32_t>(HttpMethod::POST));
	f.peer.SendString(WAS_COMMAND_URI, "/foo");
	f.peer.Send(WAS_COMMAND_NO_DATA);
	f.server.Run();

	ExpectProtocolError(f.server);
}

/**
 * Likewise for a METHOD packet following an explicit GET, which is
 * indistinguishable from the default and therefore not detected either.
 */
TEST(WasServerProtocol, MethodAfterExplicitGet)
{
	[[maybe_unused]] const ScopeInitDefaultFifoBuffer init;

	Fixture f{RecordingRequestHandler::Mode::RESPOND};

	f.peer.Send(WAS_COMMAND_REQUEST);
	f.peer.SendU32(WAS_COMMAND_METHOD,
		       static_cast<uint32_t>(HttpMethod::GET));
	f.peer.SendU32(WAS_COMMAND_METHOD,
		       static_cast<uint32_t>(HttpMethod::POST));
	f.peer.SendString(WAS_COMMAND_URI, "/foo");
	f.peer.Send(WAS_COMMAND_NO_DATA);
	f.server.Run();

	ExpectProtocolError(f.server);
}

/**
 * A METHOD packet which contradicts an earlier one must be rejected.
 */
TEST(WasServerProtocol, ConflictingMethod)
{
	[[maybe_unused]] const ScopeInitDefaultFifoBuffer init;

	Fixture f{RecordingRequestHandler::Mode::RESPOND};

	f.peer.Send(WAS_COMMAND_REQUEST);
	f.peer.SendU32(WAS_COMMAND_METHOD,
		       static_cast<uint32_t>(HttpMethod::POST));
	f.peer.SendU32(WAS_COMMAND_METHOD,
		       static_cast<uint32_t>(HttpMethod::PUT));
	f.server.Run();

	ExpectProtocolError(f.server);
}

/**
 * Unknown command codes are silently ignored for forward compatibility.
 * This is deliberate rather than an oversight, so pin it down.
 */
TEST(WasServerProtocol, UnknownCommandIgnored)
{
	[[maybe_unused]] const ScopeInitDefaultFifoBuffer init;

	Fixture f{RecordingRequestHandler::Mode::RESPOND};

	f.peer.Send(WAS_COMMAND_REQUEST);
	f.peer.SendRaw(0x7fff, AsBytes(std::string_view{"future"}));
	f.peer.SendString(WAS_COMMAND_URI, "/foo");
	f.peer.Send(WAS_COMMAND_NO_DATA);
	f.server.Run();

	EXPECT_FALSE(f.server.error);
	EXPECT_EQ(f.request_handler.n_requests, 1u);
	EXPECT_EQ(f.request_handler.uri, "/foo");
}

/**
 * A response to HEAD must be sent with NO_DATA even if the request
 * handler supplied a body.
 */
TEST(WasServerProtocol, HeadResponseWithBody)
{
	[[maybe_unused]] const ScopeInitDefaultFifoBuffer init;

	Fixture f{RecordingRequestHandler::Mode::RESPOND};
	f.request_handler.response_body = "hello";
	f.request_handler.have_response_body = true;

	f.peer.Send(WAS_COMMAND_REQUEST);
	f.peer.SendU32(WAS_COMMAND_METHOD,
		       static_cast<uint32_t>(HttpMethod::HEAD));
	f.peer.SendString(WAS_COMMAND_URI, "/foo");
	f.peer.Send(WAS_COMMAND_NO_DATA);
	f.server.Run();

	const auto packets = f.peer.Receive();
	EXPECT_TRUE(FindPacket(packets, WAS_COMMAND_NO_DATA));
	EXPECT_FALSE(FindPacket(packets, WAS_COMMAND_DATA));
}

/**
 * A second REQUEST packet inside a request must be rejected.
 */
TEST(WasServerProtocol, DuplicateRequest)
{
	[[maybe_unused]] const ScopeInitDefaultFifoBuffer init;

	Fixture f{RecordingRequestHandler::Mode::RESPOND};

	f.peer.Send(WAS_COMMAND_REQUEST);
	f.peer.Send(WAS_COMMAND_REQUEST);
	f.server.Run();

	ExpectProtocolError(f.server);
}

/**
 * STATUS is a response packet; a server must reject it.
 */
TEST(WasServerProtocol, MisplacedStatus)
{
	[[maybe_unused]] const ScopeInitDefaultFifoBuffer init;

	Fixture f{RecordingRequestHandler::Mode::RESPOND};

	f.peer.Send(WAS_COMMAND_REQUEST);
	f.peer.SendU32(WAS_COMMAND_STATUS, 200);
	f.server.Run();

	ExpectProtocolError(f.server);
}

/**
 * A METHOD packet with a bad payload size must be rejected.
 */
TEST(WasServerProtocol, MalformedMethod)
{
	[[maybe_unused]] const ScopeInitDefaultFifoBuffer init;

	Fixture f{RecordingRequestHandler::Mode::RESPOND};

	f.peer.Send(WAS_COMMAND_REQUEST);
	f.peer.SendString(WAS_COMMAND_METHOD, "xy");
	f.server.Run();

	ExpectProtocolError(f.server);
}

/**
 * A HEADER packet without a '=' separator must be rejected.
 */
TEST(WasServerProtocol, MalformedHeader)
{
	[[maybe_unused]] const ScopeInitDefaultFifoBuffer init;

	Fixture f{RecordingRequestHandler::Mode::RESPOND};

	f.peer.Send(WAS_COMMAND_REQUEST);
	f.peer.SendString(WAS_COMMAND_HEADER, "no-equals-sign");
	f.server.Run();

	ExpectProtocolError(f.server);
}

/**
 * A PARAMETER packet without a '=' separator must be rejected.
 */
TEST(WasServerProtocol, MalformedParameter)
{
	[[maybe_unused]] const ScopeInitDefaultFifoBuffer init;

	Fixture f{RecordingRequestHandler::Mode::RESPOND};

	f.peer.Send(WAS_COMMAND_REQUEST);
	f.peer.SendString(WAS_COMMAND_PARAMETER, "no-equals-sign");
	f.server.Run();

	ExpectProtocolError(f.server);
}

/**
 * A LENGTH packet with a bad payload size must be rejected.
 */
TEST(WasServerProtocol, MalformedLength)
{
	[[maybe_unused]] const ScopeInitDefaultFifoBuffer init;

	Fixture f{RecordingRequestHandler::Mode::RESPOND};

	f.peer.Send(WAS_COMMAND_REQUEST);
	f.peer.SendString(WAS_COMMAND_URI, "/foo");
	f.peer.Send(WAS_COMMAND_DATA);
	f.peer.SendU32(WAS_COMMAND_LENGTH, 4);
	f.server.Run();

	ExpectProtocolError(f.server);
}

/**
 * A PREMATURE packet with a bad payload size must be rejected.
 */
TEST(WasServerProtocol, MalformedPremature)
{
	[[maybe_unused]] const ScopeInitDefaultFifoBuffer init;

	Fixture f{RecordingRequestHandler::Mode::RESPOND};

	f.peer.SendU32(WAS_COMMAND_PREMATURE, 0);
	f.server.Run();

	ExpectProtocolError(f.server);
}

/**
 * DATA before the URI has been received must be rejected.
 */
TEST(WasServerProtocol, DataWithoutUri)
{
	[[maybe_unused]] const ScopeInitDefaultFifoBuffer init;

	Fixture f{RecordingRequestHandler::Mode::RESPOND};

	f.peer.Send(WAS_COMMAND_REQUEST);
	f.peer.Send(WAS_COMMAND_DATA);
	f.server.Run();

	ExpectProtocolError(f.server);
}

/**
 * NO_DATA before the URI has been received must be rejected.
 */
TEST(WasServerProtocol, NoDataWithoutUri)
{
	[[maybe_unused]] const ScopeInitDefaultFifoBuffer init;

	Fixture f{RecordingRequestHandler::Mode::RESPOND};

	f.peer.Send(WAS_COMMAND_REQUEST);
	f.peer.Send(WAS_COMMAND_NO_DATA);
	f.server.Run();

	ExpectProtocolError(f.server);
}

/**
 * LENGTH without a preceding DATA packet must be rejected.
 */
TEST(WasServerProtocol, LengthWithoutData)
{
	[[maybe_unused]] const ScopeInitDefaultFifoBuffer init;

	Fixture f{RecordingRequestHandler::Mode::RESPOND};

	f.peer.Send(WAS_COMMAND_REQUEST);
	f.peer.SendString(WAS_COMMAND_URI, "/foo");
	f.peer.SendU64(WAS_COMMAND_LENGTH, 4);
	f.server.Run();

	ExpectProtocolError(f.server);
}

/**
 * A second URI packet must be rejected.
 */
TEST(WasServerProtocol, DuplicateUri)
{
	[[maybe_unused]] const ScopeInitDefaultFifoBuffer init;

	Fixture f{RecordingRequestHandler::Mode::RESPOND};

	f.peer.Send(WAS_COMMAND_REQUEST);
	f.peer.SendString(WAS_COMMAND_URI, "/foo");
	f.peer.SendString(WAS_COMMAND_URI, "/bar");
	f.server.Run();

	ExpectProtocolError(f.server);
}

/**
 * STOP on an idle connection must be answered with PREMATURE 0.
 */
TEST(WasServerProtocol, StopWhileIdle)
{
	[[maybe_unused]] const ScopeInitDefaultFifoBuffer init;

	Fixture f{RecordingRequestHandler::Mode::RESPOND};

	f.peer.Send(WAS_COMMAND_STOP);
	f.server.Run();

	const auto packets = f.peer.Receive();
	const auto *premature = FindPacket(packets, WAS_COMMAND_PREMATURE);
	ASSERT_TRUE(premature);
	ASSERT_EQ(premature->payload.size(), sizeof(uint64_t));

	const auto value = LoadUnaligned<uint64_t>(premature->payload.data());
	EXPECT_EQ(value, 0u);

	EXPECT_FALSE(f.server.error);
	EXPECT_TRUE(f.server.IsAlive());
}

/**
 * A complete request/response round trip, as a sanity check of the raw
 * peer harness.
 */
TEST(WasServerProtocol, RoundTrip)
{
	[[maybe_unused]] const ScopeInitDefaultFifoBuffer init;

	Fixture f{RecordingRequestHandler::Mode::RESPOND};
	f.request_handler.response_headers.emplace("x-test", "1");

	f.peer.Send(WAS_COMMAND_REQUEST);
	f.peer.SendU32(WAS_COMMAND_METHOD,
		       static_cast<uint32_t>(HttpMethod::POST));
	f.peer.SendString(WAS_COMMAND_URI, "/foo");
	f.peer.SendString(WAS_COMMAND_SCRIPT_NAME, "/foo");
	f.peer.SendString(WAS_COMMAND_QUERY_STRING, "a=b");
	f.peer.SendString(WAS_COMMAND_HEADER, "hello=world");
	f.peer.Send(WAS_COMMAND_DATA);
	f.peer.SendU64(WAS_COMMAND_LENGTH, 5);
	f.peer.WriteBody("hello");
	f.server.Run();

	EXPECT_FALSE(f.server.error);
	EXPECT_EQ(f.request_handler.n_requests, 1u);
	EXPECT_EQ(f.request_handler.method, HttpMethod::POST);
	EXPECT_EQ(f.request_handler.uri, "/foo");
	EXPECT_EQ(f.request_handler.body, "hello");

	const auto packets = f.peer.Receive();
	const auto *status = FindPacket(packets, WAS_COMMAND_STATUS);
	ASSERT_TRUE(status);
	ASSERT_EQ(status->payload.size(), sizeof(HttpStatus));

	const auto status_value = LoadUnaligned<HttpStatus>(status->payload.data());
	EXPECT_EQ(status_value, HttpStatus::OK);

	EXPECT_TRUE(FindPacket(packets, WAS_COMMAND_HEADER));
	EXPECT_TRUE(FindPacket(packets, WAS_COMMAND_NO_DATA));
}
