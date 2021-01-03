/*
 * Copyright 2007-2021 CM4all GmbH
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

#include "NetstringInput.hxx"
#include "io/FileDescriptor.hxx"
#include "system/Error.hxx"
#include "util/RuntimeError.hxx"
#include "util/CharUtil.hxx"

#include <unistd.h>
#include <string.h>
#include <errno.h>

gcc_pure
static bool
OnlyDigits(const char *p, size_t size) noexcept
{
	for (size_t i = 0; i != size; ++i)
		if (!IsDigitASCII(p[i]))
			return false;

	return true;
}

inline NetstringInput::Result
NetstringInput::ReceiveHeader(FileDescriptor fd)
{
	ssize_t nbytes = fd.Read(header_buffer + header_position,
				 sizeof(header_buffer) - header_position);
	if (nbytes < 0) {
		switch (errno) {
		case EAGAIN:
		case EINTR:
			return Result::MORE;

		case ECONNRESET:
			return Result::CLOSED;

		default:
			throw MakeErrno("read() failed");
		}
	}

	if (nbytes == 0)
		return Result::CLOSED;

	header_position += nbytes;

	char *colon = (char *)memchr(header_buffer + 1, ':', header_position - 1);
	if (colon == nullptr) {
		if (header_position == sizeof(header_buffer) ||
		    !OnlyDigits(header_buffer, header_position))
			throw std::runtime_error("Malformed netstring");

		return Result::MORE;
	}

	*colon = 0;
	char *endptr;
	size_t size = strtoul(header_buffer, &endptr, 10);
	if (endptr != colon)
		throw std::runtime_error("Malformed netstring");

	if (size >= max_size)
		throw FormatRuntimeError("Netstring is too large: %zu", size);

	/* allocate only extra byte for the trailing comma */
	value.ResizeDiscard(size + 1);
	state = State::VALUE;
	value_position = 0;

	size_t vbytes = header_position - (colon - header_buffer) - 1;
	memcpy(&value.front(), colon + 1, vbytes);
	return ValueData(vbytes);
}

NetstringInput::Result
NetstringInput::ValueData(size_t nbytes)
{
	assert(state == State::VALUE);

	value_position += nbytes;

	if (value_position >= value.size()) {
		if (value.back() != ',')
			throw std::runtime_error("Malformed netstring");

		/* erase the trailing comma */
		value.SetSize(value.size() - 1);

		state = State::FINISHED;
		return Result::FINISHED;
	}

	return Result::MORE;
}

inline NetstringInput::Result
NetstringInput::ReceiveValue(FileDescriptor fd)
{
	ssize_t nbytes = fd.Read(&value.front() + value_position,
				 value.size() - value_position);
	if (nbytes < 0) {
		switch (errno) {
		case EAGAIN:
		case EINTR:
			return Result::MORE;

		case ECONNRESET:
			return Result::CLOSED;

		default:
			throw MakeErrno("read() failed");
		}
	}

	if (nbytes == 0)
		return Result::CLOSED;

	return ValueData(nbytes);
}

NetstringInput::Result
NetstringInput::Receive(FileDescriptor fd)
{
	switch (state) {
	case State::FINISHED:
		assert(false);
		gcc_unreachable();

	case State::HEADER:
		return ReceiveHeader(fd);

	case State::VALUE:
		return ReceiveValue(fd);
	}

	gcc_unreachable();
}
