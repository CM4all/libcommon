// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Control.hxx"
#include "Error.hxx"
#include "system/Error.hxx"

#include <was/protocol.h>

#include <string.h>

#include <stdio.h>
#include <unistd.h>

namespace Was {

Control::Control(EventLoop &event_loop, SocketDescriptor _fd,
		 ControlHandler &_handler) noexcept
	:socket(event_loop), handler(_handler)
{
	socket.Init(_fd, FD_SOCKET, write_timeout, *this);

	socket.ScheduleRead();
}

void
Control::ScheduleWrite() noexcept
{
	socket.ScheduleWrite();
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
	InvokeError(std::make_exception_ptr(WasProtocolError(msg)));
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
		const auto header = (const struct was_header *)(const void *)r.data();
		if (r.size() < sizeof(*header) ||
		    r.size() < sizeof(*header) + header->length) {
			/* not enough data yet */
			if (!InvokeDrained())
				return BufferedResult::DESTROYED;

			return BufferedResult::MORE;
		}

		const std::byte *payload = (const std::byte *)(header + 1);

		socket.KeepConsumed(sizeof(*header) + header->length);

		bool success = handler.OnWasControlPacket(was_command(header->command),
							  {payload, header->length});
		if (!success)
			return BufferedResult::DESTROYED;
	}
}

bool
Control::OnBufferedClosed() noexcept
{
	// TODO: does this need to be a fatal condition?
	InvokeError("WAS control socket closed by peer");
	return false;
}

bool
Control::OnBufferedWrite()
{
	auto r = output_buffer.Read();
	assert(!r.empty());

	ssize_t nbytes = socket.Write(r.data(), r.size());
	if (nbytes <= 0) {
		if (nbytes == WRITE_ERRNO)
			throw MakeErrno("WAS control send error");
		return true;
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
		ScheduleWrite();

	return true;
}

bool
Control::OnBufferedDrained() noexcept
{
	return handler.OnWasControlDrained();
}

void
Control::OnBufferedError(std::exception_ptr e) noexcept
{
	InvokeError(e);
}

bool
Control::FlushOutput() noexcept
{
	if (output_buffer.empty())
		return true;

	try {
		if (!OnBufferedWrite())
			return false;
	} catch (...) {
		InvokeError(std::current_exception());
		return false;
	}

	if (!output_buffer.empty())
		InvokeError("Failed to flush control channel");

	return true;
}

/*
 * constructor
 *
 */

void *
Control::Start(enum was_command cmd, size_t payload_length) noexcept
{
	assert(!done);

	output_buffer.AllocateIfNull();
	auto w = output_buffer.Write();
	struct was_header *header = (struct was_header *)(void *)w.data();
	if (w.size() < sizeof(*header) + payload_length) {
		InvokeError("control output is too large");
		return nullptr;
	}

	header->command = cmd;
	header->length = payload_length;

	return header + 1;
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
	      const void *payload, size_t payload_length) noexcept
{
	assert(!done);

	void *dest = Start(cmd, payload_length);
	if (dest == nullptr)
		return false;

	memcpy(dest, payload, payload_length);
	Finish(payload_length);
	return true;
}

bool
Control::SendString(enum was_command cmd, std::string_view payload) noexcept
{
	return Send(cmd, payload.data(), payload.size());
}

bool
Control::SendPair(enum was_command cmd, std::string_view name,
		  std::string_view value) noexcept
{
	const std::size_t payload_size = name.size() + 1 + value.size();

	char *dest = (char *)Start(cmd, payload_size);
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
