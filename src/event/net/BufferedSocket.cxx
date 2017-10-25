/*
 * Copyright 2007-2017 Content Management AG
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

#include "BufferedSocket.hxx"
#include "system/Error.hxx"
#include "net/SocketProtocolError.hxx"
#include "util/ConstBuffer.hxx"

#include <utility>

#include <errno.h>

bool
BufferedSocketHandler::OnBufferedTimeout() noexcept
{
	OnBufferedError(std::make_exception_ptr(SocketTimeoutError()));
	return false;
}

void
BufferedSocket::ClosedPrematurely() noexcept
{
	handler->OnBufferedError(std::make_exception_ptr(SocketClosedPrematurelyError()));
}

void
BufferedSocket::Ended() noexcept
{
	assert(!IsConnected());
	assert(!ended);

#ifndef NDEBUG
	ended = true;
#endif

	if (!handler->OnBufferedEnd())
		ClosedPrematurely();
}

bool
BufferedSocket::ClosedByPeer() noexcept
{
	if (expect_more) {
		ClosedPrematurely();
		return false;
	}

	const size_t remaining = input.GetAvailable();

	if (!handler->OnBufferedClosed() ||
	    !handler->OnBufferedRemaining(remaining))
		return false;

	assert(!IsConnected());
	assert(remaining == input.GetAvailable());

	if (input.IsEmpty()) {
		Ended();
		return false;
	}

	return true;
}

int
BufferedSocket::AsFD() noexcept
{
	if (!IsEmpty())
		/* can switch to the raw socket descriptor only if the input
		   buffer is empty */
		return -1;

	return base.AsFD();
}

size_t
BufferedSocket::GetAvailable() const noexcept
{
	assert(!ended);

	return input.GetAvailable();
}

void
BufferedSocket::Consumed(size_t nbytes) noexcept
{
	assert(!ended);

	input.Consume(nbytes);
}

/**
 * Invokes the data handler, and takes care for
 * #BufferedResult::AGAIN_OPTIONAL and #BufferedResult::AGAIN_EXPECT.
 */
inline BufferedResult
BufferedSocket::InvokeData() noexcept
{
	assert(!IsEmpty());

	bool local_expect_more = false;

	while (true) {
		auto r = input.Read();
		if (r.empty())
			return expect_more || local_expect_more
				? BufferedResult::MORE
				: BufferedResult::OK;

#ifndef NDEBUG
		DestructObserver destructed(*this);
#endif

		BufferedResult result;

		try {
			result = handler->OnBufferedData(r.data, r.size);
		} catch (...) {
			assert(!destructed);

			handler->OnBufferedError(std::current_exception());
			return BufferedResult::CLOSED;
		}

#ifndef NDEBUG
		if (destructed) {
			assert(result == BufferedResult::CLOSED);
		} else {
			last_buffered_result = result;
			assert((result == BufferedResult::CLOSED) == !IsValid());
		}
#endif

		if (result == BufferedResult::AGAIN_EXPECT)
			local_expect_more = true;
		else if (result == BufferedResult::AGAIN_OPTIONAL)
			local_expect_more = false;
		else
			return result;
	}
}

bool
BufferedSocket::SubmitFromBuffer() noexcept
{
	if (IsEmpty())
		return true;

	const bool old_expect_more = expect_more;
	expect_more = false;

	BufferedResult result = InvokeData();
	assert((result == BufferedResult::CLOSED) || IsValid());

	switch (result) {
	case BufferedResult::OK:
		assert(input.IsEmpty());
		assert(!expect_more);

		input.Free();

		if (!IsConnected()) {
			Ended();
			return false;
		}

		if (!base.IsReadPending())
			/* try to refill the buffer, now that it's become empty
			   (but don't refresh the pending timeout) */
			base.ScheduleRead(read_timeout);

		return true;

	case BufferedResult::PARTIAL:
		assert(!input.IsEmpty());

		if (!IsConnected())
			return false;

		return true;

	case BufferedResult::MORE:
		expect_more = true;

		if (!IsConnected()) {
			ClosedPrematurely();
			return false;
		}

		if (IsFull()) {
			handler->OnBufferedError(std::make_exception_ptr(SocketBufferFullError()));
			return false;
		}

		input.FreeIfEmpty();

		if (!base.IsReadPending())
			/* reschedule the read event just in case the buffer was
			   full before and some data has now been consumed (but
			   don't refresh the pending timeout) */
			base.ScheduleRead(read_timeout);

		return true;

	case BufferedResult::AGAIN_OPTIONAL:
	case BufferedResult::AGAIN_EXPECT:
		/* unreachable, has been handled by InvokeData() */
		assert(false);
		gcc_unreachable();

	case BufferedResult::BLOCKING:
		expect_more = old_expect_more;

		if (input.IsFull())
			/* our input buffer is still full - unschedule all reads,
			   and wait for somebody to request more data */
			UnscheduleRead();

		return false;

	case BufferedResult::CLOSED:
		/* the BufferedSocket object has been destroyed by the
		   handler */
		return false;
	}

	assert(false);
	gcc_unreachable();
}

/**
 * @return true if more data should be read from the socket
 */
inline bool
BufferedSocket::SubmitDirect() noexcept
{
	assert(IsConnected());
	assert(IsEmpty());

	const bool old_expect_more = expect_more;
	expect_more = false;

	DirectResult result;

	try {
		result = handler->OnBufferedDirect(base.GetFD(), base.GetType());
	} catch (...) {
		handler->OnBufferedError(std::current_exception());
		return false;
	}

	switch (result) {
	case DirectResult::OK:
		/* some data was transferred: refresh the read timeout */
		base.ScheduleRead(read_timeout);
		return true;

	case DirectResult::BLOCKING:
		expect_more = old_expect_more;
		UnscheduleRead();
		return false;

	case DirectResult::EMPTY:
		/* schedule read, but don't refresh timeout of old scheduled
		   read */
		if (!base.IsReadPending())
			base.ScheduleRead(read_timeout);
		return true;

	case DirectResult::END:
		Ended();
		return false;

	case DirectResult::CLOSED:
		return false;

	case DirectResult::ERRNO:
		handler->OnBufferedError(std::make_exception_ptr(MakeErrno()));
		return false;
	}

	assert(false);
	gcc_unreachable();
}

inline bool
BufferedSocket::FillBuffer() noexcept
{
	assert(IsConnected());

	if (input.IsNull())
		input.Allocate();

	ssize_t nbytes = base.ReadToBuffer(input);
	if (gcc_likely(nbytes > 0)) {
		/* success: data was added to the buffer */
		expect_more = false;
		got_data = true;

		return true;
	}

	if (nbytes == 0)
		/* socket closed */
		return ClosedByPeer();

	if (nbytes == -2) {
		/* input buffer is full */
		UnscheduleRead();
		return true;
	}

	if (nbytes == -1) {
		if (errno == EAGAIN) {
			input.FreeIfEmpty();

			/* schedule read, but don't refresh timeout of old
			   scheduled read */
			if (!base.IsReadPending())
				base.ScheduleRead(read_timeout);
			return true;
		} else {
			handler->OnBufferedError(std::make_exception_ptr(MakeErrno("recv() failed")));
			return false;
		}
	}

	return true;
}

inline bool
BufferedSocket::TryRead2() noexcept
{
	assert(IsValid());
	assert(!destroyed);
	assert(!ended);
	assert(reading);

	if (!IsConnected()) {
		assert(!IsEmpty());

		SubmitFromBuffer();
		return false;
	} else if (direct) {
		/* empty the remaining buffer before doing direct transfer */
		if (!SubmitFromBuffer())
			return false;

		if (!direct)
			/* meanwhile, the "direct" flag was reverted by the
			   handler - try again */
			return TryRead2();

		if (!IsEmpty()) {
			/* there's still data in the buffer, but our handler isn't
			   ready for consuming it - stop reading from the
			   socket */
			UnscheduleRead();
			return true;
		}

		return SubmitDirect();
	} else {
		got_data = false;

		if (!FillBuffer())
			return false;

		if (!SubmitFromBuffer())
			return false;

		if (got_data)
			/* refresh the timeout each time data was received */
			base.ScheduleRead(read_timeout);
		return true;
	}
}

bool
BufferedSocket::TryRead() noexcept
{
	assert(IsValid());
	assert(!destroyed);
	assert(!ended);
	assert(!reading);

#ifndef NDEBUG
	DestructObserver destructed(*this);
	reading = true;
#endif

	const bool result = TryRead2();

#ifndef NDEBUG
	if (!destructed) {
		assert(reading);
		reading = false;
	}
#endif

	return result;
}

/*
 * socket_wrapper handler
 *
 */

bool
BufferedSocket::OnSocketWrite() noexcept
{
	assert(!destroyed);
	assert(!ended);

	try {
		return handler->OnBufferedWrite();
	} catch (...) {
		handler->OnBufferedError(std::current_exception());
		return false;
	}
}

bool
BufferedSocket::OnSocketRead() noexcept
{
	assert(!destroyed);
	assert(!ended);

	return TryRead();
}

bool
BufferedSocket::OnSocketTimeout() noexcept
{
	assert(!destroyed);
	assert(!ended);

	return handler->OnBufferedTimeout();
}

/*
 * public API
 *
 */

void
BufferedSocket::Init(SocketDescriptor _fd, FdType _fd_type,
		     const struct timeval *_read_timeout,
		     const struct timeval *_write_timeout,
		     BufferedSocketHandler &_handler) noexcept
{
	base.Init(_fd, _fd_type);

	read_timeout = _read_timeout;
	write_timeout = _write_timeout;

	handler = &_handler;
	input.SetNull();
	direct = false;
	expect_more = false;
	destroyed = false;

#ifndef NDEBUG
	reading = false;
	ended = false;
	last_buffered_result = BufferedResult(-1);
#endif
}

void
BufferedSocket::Reinit(const struct timeval *_read_timeout,
		       const struct timeval *_write_timeout,
		       BufferedSocketHandler &_handler) noexcept
{
	assert(IsValid());
	assert(IsConnected());
	assert(!expect_more);

	read_timeout = _read_timeout;
	write_timeout = _write_timeout;

	handler = &_handler;

	direct = false;
}

void
BufferedSocket::Init(BufferedSocket &&src,
		     const struct timeval *_read_timeout,
		     const struct timeval *_write_timeout,
		     BufferedSocketHandler &_handler) noexcept
{
	base.Init(std::move(src.base));

	read_timeout = _read_timeout;
	write_timeout = _write_timeout;

	handler = &_handler;

	/* steal the input buffer (after we already stole the socket) */
	input = std::move(src.input);

	direct = false;
	expect_more = false;
	destroyed = false;

#ifndef NDEBUG
	reading = false;
	ended = false;
	last_buffered_result = BufferedResult(-1);
#endif
}

void
BufferedSocket::Destroy() noexcept
{
	assert(!base.IsValid());
	assert(!destroyed);

	input.FreeIfDefined();

	destroyed = true;
}

bool
BufferedSocket::IsEmpty() const noexcept
{
	assert(!ended);

	return input.IsEmpty();
}

bool
BufferedSocket::IsFull() const noexcept
{
	assert(!ended);

	return input.IsDefinedAndFull();
}

bool
BufferedSocket::Read(bool _expect_more) noexcept
{
	assert(!reading);
	assert(!destroyed);
	assert(!ended);

	if (_expect_more) {
		if (!IsConnected() && IsEmpty()) {
			ClosedPrematurely();
			return false;
		}

		expect_more = true;
	}

	return TryRead();
}

ssize_t
BufferedSocket::Write(const void *data, size_t length) noexcept
{
	ssize_t nbytes = base.Write(data, length);

	if (gcc_unlikely(nbytes < 0)) {
		const int e = errno;
		if (gcc_likely(e == EAGAIN)) {
			ScheduleWrite();
			return WRITE_BLOCKING;
		} else if (e == EPIPE || e == ECONNRESET) {
			enum write_result r = handler->OnBufferedBroken();

			if (r == WRITE_BROKEN)
				UnscheduleWrite();

			nbytes = ssize_t(r);
		}
	}

	return nbytes;
}

ssize_t
BufferedSocket::WriteV(const struct iovec *v, size_t n) noexcept
{
	ssize_t nbytes = base.WriteV(v, n);

	if (gcc_unlikely(nbytes < 0)) {
		const int e = errno;
		if (gcc_likely(e == EAGAIN)) {
			ScheduleWrite();
			return WRITE_BLOCKING;
		} else if (e == EPIPE || e == ECONNRESET) {
			enum write_result r = handler->OnBufferedBroken();
			if (r == WRITE_BROKEN)
				UnscheduleWrite();

			nbytes = ssize_t(r);
		}
	}

	return nbytes;
}

ssize_t
BufferedSocket::WriteFrom(int other_fd, FdType other_fd_type,
			  size_t length) noexcept
{
	ssize_t nbytes = base.WriteFrom(other_fd, other_fd_type, length);
	if (gcc_unlikely(nbytes < 0)) {
		const int e = errno;
		if (gcc_likely(e == EAGAIN)) {
			if (!IsReadyForWriting()) {
				ScheduleWrite();
				return WRITE_BLOCKING;
			}

			/* try again, just in case our fd has become ready between
			   the first socket_wrapper_write_from() call and
			   IsReadyForWriting() */
			nbytes = base.WriteFrom(other_fd, other_fd_type, length);
		}
	}

	return nbytes;
}

void
BufferedSocket::DeferRead(bool _expect_more) noexcept
{
	assert(!ended);
	assert(!destroyed);

	if (_expect_more)
		expect_more = true;

	defer_read.Schedule();
}

void
BufferedSocket::ScheduleReadTimeout(bool _expect_more,
				    const struct timeval *timeout) noexcept
{
	assert(!ended);
	assert(!destroyed);

	if (_expect_more)
		expect_more = true;

	read_timeout = timeout;

	if (!input.IsEmpty())
		/* deferred call to Read() to deliver data from the buffer */
		defer_read.Schedule();
	else
		/* the input buffer is empty: wait for more data from the
		   socket */
		base.ScheduleRead(timeout);
}
