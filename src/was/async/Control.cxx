// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Control.hxx"
#include "net/SocketProtocolError.hxx"
#include "system/Error.hxx"
#include "util/SpanCast.hxx"
#include "util/Unaligned.hxx"

#include <was/protocol.h>

namespace Was {

Control::Control(EventLoop &event_loop, SocketDescriptor _fd,
		 ControlHandler &_handler) noexcept
	:socket(event_loop), handler(&_handler)
{
	socket.Init(_fd, FD_SOCKET, write_timeout, *this);

	if (!socket.HasUring())
		socket.ScheduleRead();
}

void
Control::ReleaseSocket() noexcept
{
	assert(socket.IsConnected());

	output_buffer.FreeIfDefined();

	socket.Abandon();
	socket.Destroy();
}

void
Control::InvokeError(const char *msg) noexcept
{
	InvokeError(std::make_exception_ptr(SocketProtocolError{msg}));
}

BufferedResult
Control::OnBufferedData()
{
	if (done) {
		InvokeError("received too much control data");
		return BufferedResult::DESTROYED;
	}

	while (true) {
		auto r = socket.ReadBuffer();

		struct was_header header;
		if (r.size() < sizeof(header))
			break;

		LoadUnaligned(header, r.data());
		r = r.subspan(sizeof(header));
		if (r.size() < header.length)
			break;

		const auto payload = r.first(header.length);

		socket.KeepConsumed(sizeof(header) + payload.size());

		bool success = handler->OnWasControlPacket(was_command(header.command),
							   payload);
		if (!success)
			return BufferedResult::DESTROYED;
	}

	/* not enough data yet */
	if (!InvokeDrained())
		return BufferedResult::DESTROYED;

	return BufferedResult::MORE;
}

bool
Control::OnBufferedClosed() noexcept
{
	Close();
	handler->OnWasControlHangup();
	return false;
}

bool
Control::OnBufferedWrite()
{
	auto r = output_buffer.Read();
	assert(!r.empty());

	ssize_t nbytes = socket.Write(r);
	if (nbytes <= 0) {
		switch (static_cast<write_result>(nbytes)) {
		case WRITE_SOURCE_EOF:
		case WRITE_BLOCKING:
			return true;

		case WRITE_ERRNO:
			throw MakeErrno("WAS control send error");

		case WRITE_DESTROYED:
		case WRITE_BROKEN:
			return false;
		}
	}

	output_buffer.Consume(nbytes);

	if (output_buffer.empty()) {
		output_buffer.Free();
		socket.UnscheduleWrite();

		if (done) {
			InvokeDone();
			return false;
		}
	} else
		socket.ScheduleWrite();

	return true;
}

bool
Control::OnBufferedDrained() noexcept
{
	return handler->OnWasControlDrained();
}

enum write_result
Control::OnBufferedBroken()
{
	throw SocketClosedPrematurelyError{"WAS control socket closed by peer"};
}

void
Control::OnBufferedError(std::exception_ptr e) noexcept
{
	InvokeError(e);
}

/*
 * constructor
 *
 */

std::byte *
Control::Start(enum was_command cmd, size_t payload_length) noexcept
{
	assert(!done);

	const struct was_header header{
		.length = static_cast<uint16_t>(payload_length),
		.command = static_cast<uint16_t>(cmd),
	};

	output_buffer.AllocateIfNull();
	auto w = output_buffer.Write();
	if (w.size() < sizeof(header) + payload_length) {
		InvokeError("control output is too large");
		return nullptr;
	}

	StoreUnaligned(w.data(), header);
	return w.data() + sizeof(header);
}

void
Control::Finish(size_t payload_length) noexcept
{
	assert(!done);

	output_buffer.Append(sizeof(struct was_header) + payload_length);

	socket.DeferWrite();
}

bool
Control::Send(enum was_command cmd,
	      std::span<const std::byte> payload) noexcept
{
	assert(!done);

	std::byte *dest = Start(cmd, payload.size());
	if (dest == nullptr)
		return false;

	std::copy(payload.begin(), payload.end(), dest);
	Finish(payload.size());
	return true;
}

bool
Control::SendString(enum was_command cmd, std::string_view payload) noexcept
{
	return Send(cmd, AsBytes(payload));
}

bool
Control::SendPair(enum was_command cmd, std::string_view name,
		  std::string_view value) noexcept
{
	const std::size_t payload_size = name.size() + 1 + value.size();

	char *dest = reinterpret_cast<char *>(Start(cmd, payload_size));
	if (dest == nullptr)
		return false;

	dest = std::copy(name.begin(), name.end(), dest);
	*dest++ = '=';
	dest = std::copy(value.begin(), value.end(), dest);

	Finish(payload_size);
	return true;
}

bool
Control::SendArray(enum was_command cmd,
		   std::span<const char *const> values) noexcept
{
	for (auto value : values) {
		assert(value != nullptr);

		if (!SendString(cmd, value))
			return false;
	}

	return true;
}

void
Control::Done() noexcept
{
	assert(!done);

	done = true;

	if (!socket.IsEmpty()) {
		InvokeError("received too much control data");
		return;
	}

	if (output_buffer.empty())
		InvokeDone();
}

} // namespace Was
