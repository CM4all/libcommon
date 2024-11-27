// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "NetstringInput.hxx"
#include "net/SocketProtocolError.hxx"
#include "io/FileDescriptor.hxx"
#include "system/Error.hxx"

#include <fmt/core.h>

#include <charconv>
#include <string_view>

#include <string.h>
#include <errno.h>

inline NetstringInput::Result
NetstringInput::ReceiveHeader(FileDescriptor fd)
{
	assert(IsReceivingHeader());
	assert(header_position < sizeof(header_buffer));

	ssize_t nbytes = fd.Read(std::as_writable_bytes(std::span{header_buffer}.subspan(header_position)));
	if (nbytes < 0) {
		switch (const int e = errno) {
		case EAGAIN:
		case EINTR:
			return Result::MORE;

		case ECONNRESET:
			return Result::CLOSED;

		default:
			throw MakeErrno(e, "read() failed");
		}
	}

	if (nbytes == 0)
		return Result::CLOSED;

	header_position += nbytes;
	const char *const header_end = header_buffer + header_position;

	std::size_t size;
	auto [end, error] = std::from_chars(header_buffer, header_end,
					    size, 10);
	if (end == header_buffer || error != std::errc{})
		throw SocketProtocolError{"Malformed netstring"};

	if (end == header_end) {
		/* no colon received yet */
		if (header_position == sizeof(header_buffer))
			throw SocketProtocolError{"Malformed netstring"};

		return Result::MORE;
	}

	if (*end != ':')
		throw SocketProtocolError{"Malformed netstring"};

	if (size > max_size)
		throw SocketMessageTooLargeError{fmt::format("Netstring is too large: {}", size)};

	const std::string_view rest{end + 1, header_end};
	if (rest.size() > size + 1)
		throw SocketGarbageReceivedError{"Garbage received after netstring"};

	/* allocate one extra byte for the trailing comma */
	value.ResizeDiscard(size + 1);
	value_position = 0;

	memcpy(value.data(), rest.data(), rest.size());
	return ValueData(rest.size());
}

NetstringInput::Result
NetstringInput::ValueData(size_t nbytes)
{
	assert(!IsReceivingHeader());

	value_position += nbytes;

	if (value_position >= value.size()) {
		if (value.back() != std::byte{','})
			throw SocketProtocolError{"Malformed netstring"};

		/* erase the trailing comma */
		value.SetSize(value.size() - 1);

#ifndef NDEBUG
		finished = true;
#endif
		return Result::FINISHED;
	}

	return Result::MORE;
}

inline NetstringInput::Result
NetstringInput::ReceiveValue(FileDescriptor fd)
{
	ssize_t nbytes = fd.Read(std::span{value}.subspan(value_position));
	if (nbytes < 0) {
		switch (const int e = errno) {
		case EAGAIN:
		case EINTR:
			return Result::MORE;

		case ECONNRESET:
			return Result::CLOSED;

		default:
			throw MakeErrno(e, "read() failed");
		}
	}

	if (nbytes == 0)
		return Result::CLOSED;

	return ValueData(nbytes);
}

NetstringInput::Result
NetstringInput::Receive(FileDescriptor fd)
{
	assert(!finished);

	if (IsReceivingHeader())
		return ReceiveHeader(fd);
	else
		return ReceiveValue(fd);
}
