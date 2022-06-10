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

#include "BufferedSocket.hxx"
#include "system/Error.hxx"
#include "net/SocketProtocolError.hxx"
#include "net/TimeoutError.hxx"

#include <errno.h>

BufferedSocket::BufferedSocket(EventLoop &_event_loop) noexcept
	:base(_event_loop, *this),
	 defer_read(_event_loop, BIND_THIS_METHOD(DeferReadCallback)),
	 defer_write(_event_loop, BIND_THIS_METHOD(DeferWriteCallback))
{
}

BufferedSocket::~BufferedSocket() noexcept = default;

bool
BufferedSocketHandler::OnBufferedTimeout() noexcept
{
	OnBufferedError(std::make_exception_ptr(TimeoutError()));
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

	const std::size_t remaining = input.GetAvailable();

	if (!handler->OnBufferedClosed() ||
	    !handler->OnBufferedRemaining(remaining))
		return false;

	assert(!IsConnected());
	assert(remaining == input.GetAvailable());

	if (input.empty()) {
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

std::size_t
BufferedSocket::GetAvailable() const noexcept
{
	assert(!ended);

	return input.GetAvailable();
}

void
BufferedSocket::DisposeConsumed(std::size_t nbytes) noexcept
{
	assert(!ended);

	if (nbytes == 0)
		/* this shouldn't happen, but if a caller passes 0 and
		   we have no buffer, the FreeIfEmpty() call may
		   crash */
		return;

	assert(input.IsDefined());

	input.Consume(nbytes);
	input.FreeIfEmpty();
}

void
BufferedSocket::KeepConsumed(std::size_t nbytes) noexcept
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
		if (input.empty())
			return expect_more || local_expect_more
				? BufferedResult::MORE
				: BufferedResult::OK;

#ifndef NDEBUG
		DestructObserver destructed(*this);
#endif

		BufferedResult result;

		try {
			result = handler->OnBufferedData();
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
			assert((result == BufferedResult::CLOSED) || IsValid());
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
		assert(!expect_more);

		if (input.empty()) {
			input.FreeIfDefined();

			if (!IsConnected()) {
				Ended();
				return false;
			}

			if (!base.IsReadPending())
				/* try to refill the buffer, now that
				   it's become empty (but don't
				   refresh the pending timeout) */
				base.ScheduleRead(read_timeout);
		} else {
			if (!IsConnected())
				return false;
		}

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
		result = handler->OnBufferedDirect(base.GetSocket(), base.GetType());
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
		return ClosedByPeer();

	case DirectResult::CLOSED:
		return false;

	case DirectResult::ERRNO:
		handler->OnBufferedError(std::make_exception_ptr(MakeErrno("splice() from socket failed")));
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

	if (nbytes == -2) {
		/* input buffer is full */
		UnscheduleRead();
		return true;
	}

	input.FreeIfEmpty();

	if (nbytes == 0)
		/* socket closed */
		return ClosedByPeer();

	if (nbytes == -1) {
		if (const int e = errno; e == EAGAIN) [[likely]] {
			/* schedule read, but don't refresh timeout of old
			   scheduled read */
			if (!base.IsReadPending())
				base.ScheduleRead(read_timeout);
			return true;
		} else {
			handler->OnBufferedError(std::make_exception_ptr(MakeErrno(e, "recv() failed")));
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

inline void
BufferedSocket::DeferWriteCallback() noexcept
{
	try {
		handler->OnBufferedWrite();
	} catch (...) {
		handler->OnBufferedError(std::current_exception());
	}
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

	/* if this is scheduled, it's obsolete, because we handle it
	   here */
	defer_write.Cancel();

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

bool
BufferedSocket::OnSocketHangup() noexcept
{
	assert(!destroyed);
	assert(!ended);

	return handler->OnBufferedHangup();
}

bool
BufferedSocket::OnSocketError(int error) noexcept
{
	if (error == EPIPE || error == ECONNRESET) {
		/* this happens when the peer does a shutdown(SHUT_RD)
		   because he's not interested in more data; now our
		   handler gets a chance to say "that's ok, but I want
		   to continue reading" */
		/* TODO: how can we exclude ECONNRESET from this
		   check?  It happened in tests after one send() had
		   returned -EPIPE, and EPOLLOUT had already been
		   removed from the mask */

		const auto result = handler->OnBufferedBroken();
		switch (result) {
		case WRITE_BROKEN:
			/* continue reading */
			UnscheduleWrite();
			return true;

		case WRITE_ERRNO:
			/* report the error to OnBufferedError() */
			break;

		case WRITE_DESTROYED:
			/* this object was destroyed; return without
			   touching anything */
			return false;

		case WRITE_BLOCKING:
		case WRITE_SOURCE_EOF:
			/* these enum values are not allowed here */
			gcc_unreachable();
		}
	}

	handler->OnBufferedError(std::make_exception_ptr(MakeErrno(error,
								   "Socket error")));
	return false;
}

/*
 * public API
 *
 */

void
BufferedSocket::Init(SocketDescriptor _fd, FdType _fd_type) noexcept
{
	base.Init(_fd, _fd_type);

	read_timeout = Event::Duration(-1);
	write_timeout = Event::Duration(-1);

	handler = nullptr;
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
BufferedSocket::Init(SocketDescriptor _fd, FdType _fd_type,
		     Event::Duration _read_timeout,
		     Event::Duration _write_timeout,
		     BufferedSocketHandler &_handler) noexcept
{
	assert(!input.IsDefined());

	base.Init(_fd, _fd_type);

	read_timeout = _read_timeout;
	write_timeout = _write_timeout;

	handler = &_handler;
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
BufferedSocket::Reinit(Event::Duration _read_timeout,
		       Event::Duration _write_timeout,
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
BufferedSocket::Destroy() noexcept
{
	assert(!base.IsValid());
	assert(!destroyed);

	input.FreeIfDefined();

	destroyed = true;
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
BufferedSocket::Write(const void *data, std::size_t length) noexcept
{
	ssize_t nbytes = base.Write(data, length);

	if (gcc_unlikely(nbytes < 0)) {
		if (const int e = errno; e == EAGAIN) [[likely]] {
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
BufferedSocket::WriteV(const struct iovec *v, std::size_t n) noexcept
{
	ssize_t nbytes = base.WriteV(v, n);

	if (gcc_unlikely(nbytes < 0)) {
		if (const int e = errno; e == EAGAIN) [[likely]] {
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
			  std::size_t length) noexcept
{
	ssize_t nbytes = base.WriteFrom(other_fd, other_fd_type, length);
	if (gcc_unlikely(nbytes < 0)) {
		if (const int e = errno; e == EAGAIN) [[likely]] {
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
				    Event::Duration timeout) noexcept
{
	assert(!ended);
	assert(!destroyed);

	if (_expect_more)
		expect_more = true;

	read_timeout = timeout;

	if (!input.empty())
		/* deferred call to Read() to deliver data from the buffer */
		defer_read.Schedule();
	else
		/* the input buffer is empty: wait for more data from the
		   socket */
		base.ScheduleRead(timeout);
}
