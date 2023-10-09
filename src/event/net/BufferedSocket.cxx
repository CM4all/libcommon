// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

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

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsuggest-attribute=noreturn"
#endif

bool
BufferedSocketHandler::OnBufferedEnd()
{
	throw SocketClosedPrematurelyError{};
}

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

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

bool
BufferedSocket::Ended() noexcept
try {
	assert(IsValid());
	assert(!IsConnected());
	assert(!ended);

#ifndef NDEBUG
	ended = true;

	DestructObserver destructed(*this);
#endif

	const bool result = handler->OnBufferedEnd();

	if (result) {
		assert(!destructed);
		assert(IsValid());
	} else {
		assert(destructed || !IsValid());
	}

	return result;
} catch (...) {
	assert(IsValid());

	handler->OnBufferedError(std::current_exception());
	return false;
}

BufferedSocket::ClosedByPeerResult
BufferedSocket::ClosedByPeer() noexcept
{
	const std::size_t remaining = input.GetAvailable();

	if (!handler->OnBufferedClosed() ||
	    !handler->OnBufferedRemaining(remaining))
		return ClosedByPeerResult::DESTROYED;

	assert(!IsConnected());
	assert(remaining == input.GetAvailable());

	if (input.empty())
		return Ended()
			? ClosedByPeerResult::ENDED
			: ClosedByPeerResult::DESTROYED;

	return ClosedByPeerResult::OK;
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
 * #BufferedResult::AGAIN.
 */
inline BufferedResult
BufferedSocket::InvokeData() noexcept
{
	assert(!IsEmpty());
	assert(!in_data_handler);

	while (true) {
		if (input.empty())
			return BufferedResult::OK;

#ifndef NDEBUG
		DestructObserver destructed(*this);
#endif

		BufferedResult result;

		assert(!in_data_handler);
		in_data_handler = true;

		try {
			result = handler->OnBufferedData();
		} catch (...) {
			assert(!destructed);

			handler->OnBufferedError(std::current_exception());
			return BufferedResult::DESTROYED;
		}

#ifndef NDEBUG
		if (destructed) {
			assert(result == BufferedResult::DESTROYED);
		} else {
			assert((result == BufferedResult::DESTROYED) || IsValid());
		}
#endif

		if (result != BufferedResult::DESTROYED) {
			assert(in_data_handler);
			in_data_handler = false;
		}

		if (result != BufferedResult::AGAIN)
			return result;
	}
}

BufferedReadResult
BufferedSocket::SubmitFromBuffer() noexcept
{
	if (IsEmpty()) {
		/* only reachable if the socket is still connected, or
		   else the BufferedSocket would have "ended"
		   already */
		assert(IsConnected());

		return BufferedReadResult::OK;
	}

#ifndef NDEBUG
	DestructObserver destructed{*this};
#endif

	BufferedResult result = InvokeData();
	assert((result == BufferedResult::DESTROYED) || IsValid());

	switch (result) {
	case BufferedResult::OK:
		if (input.empty()) {
			input.FreeIfDefined();

			if (!IsConnected())
				return Ended()
					? BufferedReadResult::DISCONNECTED
					: BufferedReadResult::DESTROYED;

			/* try to refill the buffer, now that it's
			   become empty */
			base.ScheduleRead();
		} else if (IsFull()) {
			if (IsConnected())
				UnscheduleRead();
		}

		return IsConnected() ? BufferedReadResult::OK : BufferedReadResult::DISCONNECTED;

	case BufferedResult::MORE:
		if (!IsConnected()) {
			ClosedPrematurely();
			return BufferedReadResult::DESTROYED;
		}

		if (IsFull()) {
			handler->OnBufferedError(std::make_exception_ptr(SocketBufferFullError()));
			return BufferedReadResult::DESTROYED;
		}

		input.FreeIfEmpty();

		/* reschedule the read event just in case the buffer
		   was full before and some data has now been
		   consumed */
		base.ScheduleRead();

		return BufferedReadResult::OK;

	case BufferedResult::AGAIN:
		/* unreachable, has been handled by InvokeData() */
		assert(false);
		gcc_unreachable();

	case BufferedResult::DESTROYED:
		/* the BufferedSocket object has been destroyed by the
		   handler */
		assert(destructed || !IsValid());
		return BufferedReadResult::DESTROYED;
	}

	assert(false);
	gcc_unreachable();
}

/**
 * @return true if more data should be read from the socket
 */
inline BufferedReadResult
BufferedSocket::SubmitDirect() noexcept
{
	assert(IsValid());
	assert(IsConnected());
	assert(IsEmpty());

#ifndef NDEBUG
	DestructObserver destructed{*this};
#endif

	DirectResult result;

	try {
		result = handler->OnBufferedDirect(base.GetSocket(), base.GetType());
	} catch (...) {
		assert(!destructed);
		assert(IsValid());

		handler->OnBufferedError(std::current_exception());
		return BufferedReadResult::DESTROYED;
	}

	switch (result) {
	case DirectResult::OK:
		/* some data was transferred: refresh the read timeout */
		assert(!destructed);
		assert(IsValid());

		base.ScheduleRead();
		return BufferedReadResult::OK;

	case DirectResult::BLOCKING:
		assert(!destructed);
		assert(IsValid());

		UnscheduleRead();
		return BufferedReadResult::BLOCKING;

	case DirectResult::EMPTY:
		assert(!destructed);
		assert(IsValid());

		base.ScheduleRead();
		return BufferedReadResult::OK;

	case DirectResult::END:
		assert(!destructed);
		assert(IsValid());

		switch (ClosedByPeer()) {
		case ClosedByPeerResult::OK:
		case ClosedByPeerResult::ENDED:
			break;

		case ClosedByPeerResult::DESTROYED:
			assert(destructed || !IsValid());
			return BufferedReadResult::DESTROYED;
		}

		assert(!destructed);
		assert(IsValid());
		assert(!IsConnected());
		return BufferedReadResult::DISCONNECTED;

	case DirectResult::CLOSED:
		assert(destructed || !IsValid());
		return BufferedReadResult::DESTROYED;

	case DirectResult::ERRNO:
		assert(!destructed);
		assert(IsValid());

		handler->OnBufferedError(std::make_exception_ptr(MakeErrno("splice() from socket failed")));
		assert(destructed || !IsValid());
		return BufferedReadResult::DESTROYED;
	}

	assert(false);
	gcc_unreachable();
}

inline BufferedSocket::FillBufferResult
BufferedSocket::FillBuffer() noexcept
{
	assert(IsConnected());

	if (input.IsNull())
		input.Allocate();

	ssize_t nbytes = base.ReadToBuffer(input);
	if (nbytes > 0) [[likely]] {
		/* success: data was added to the buffer */
		return FillBufferResult::RECEIVED;
	}

	if (nbytes == -2) {
		/* input buffer is full */
		return FillBufferResult::BUFFER_FULL;
	}

	input.FreeIfEmpty();

	if (nbytes == 0) {
		/* socket closed */
		switch (ClosedByPeer()) {
		case ClosedByPeerResult::OK:
			return FillBufferResult::DISCONNECTED;

		case ClosedByPeerResult::ENDED:
			return FillBufferResult::ENDED;

		case ClosedByPeerResult::DESTROYED:
			return FillBufferResult::DESTROYED;
		}
	}

	if (nbytes == -1) {
		if (const int e = errno; e == EAGAIN) [[likely]] {
			return FillBufferResult::NOT_READY;
		} else {
			handler->OnBufferedError(std::make_exception_ptr(MakeErrno(e, "recv() failed")));
			return FillBufferResult::DESTROYED;
		}
	}

	// TODO this is unreachable
	return FillBufferResult::RECEIVED;
}

inline BufferedReadResult
BufferedSocket::TryRead2() noexcept
{
	assert(IsValid());
	assert(!destroyed);
	assert(!ended);
	assert(reading);

#ifndef NDEBUG
	DestructObserver destructed{*this};
#endif

	if (!IsConnected()) {
		if (IsEmpty()) {
			/* this can only happen if
			   DisposeConsumed()/KeepConsumed() was called
			   outside of the OnBufferedData() callback;
			   now's the time for the "ended" check */
			return Ended()
				? BufferedReadResult::DISCONNECTED
				: BufferedReadResult::DESTROYED;
		}

		return SubmitFromBuffer();
	} else if (direct) {
		/* empty the remaining buffer before doing direct transfer */
		switch (auto result = SubmitFromBuffer()) {
		case BufferedReadResult::OK:
			assert(!destructed);
			assert(IsValid());
			assert(IsConnected());
			break;

		case BufferedReadResult::BLOCKING:
			assert(!destructed);
			assert(IsValid());
			assert(IsConnected());
			return result;

		case BufferedReadResult::DISCONNECTED:
			assert(!destructed);
			assert(IsValid());
			assert(!IsConnected());
			return result;

		case BufferedReadResult::DESTROYED:
			assert(destructed || !IsValid());
			return result;
		}

		if (!direct)
			/* meanwhile, the "direct" flag was reverted by the
			   handler - try again */
			return TryRead2();

		if (!IsEmpty()) {
			/* there's still data in the buffer, but our handler isn't
			   ready for consuming it - stop reading from the
			   socket */
			UnscheduleRead();
			return BufferedReadResult::OK;
		}

		return SubmitDirect();
	} else {
		switch (FillBuffer()) {
		case FillBufferResult::RECEIVED:
		case FillBufferResult::BUFFER_FULL:
		case FillBufferResult::NOT_READY:
			/* the socket is still connected and there may
			   (or may not) be data in the input buffer */
			assert(!destructed);
			assert(IsValid());
			assert(IsConnected());
			break;

		case FillBufferResult::DISCONNECTED:
			/* the socket was just disconnected, but there
			   is still data in the input buffer */
			assert(!destructed);
			assert(IsValid());
			assert(!IsConnected());
			break;

		case FillBufferResult::ENDED:
			/* the socket was just disconnected and there
			   is no more data in the input buffer */
			assert(!destructed);
			assert(IsValid());
			assert(!IsConnected());
			return BufferedReadResult::DISCONNECTED;

		case FillBufferResult::DESTROYED:
			assert(destructed || !IsValid());
			return BufferedReadResult::DESTROYED;
		}

		switch (auto result = SubmitFromBuffer()) {
		case BufferedReadResult::OK:
			assert(!destructed);
			assert(IsValid());
			assert(IsConnected());
			break;

		case BufferedReadResult::BLOCKING:
			assert(!destructed);
			assert(IsValid());
			assert(IsConnected());
			return result;

		case BufferedReadResult::DISCONNECTED:
			assert(!destructed);
			assert(IsValid());
			assert(!IsConnected());
			return result;

		case BufferedReadResult::DESTROYED:
			assert(destructed || !IsValid());
			return result;
		}

		if (IsConnected()) {
			if (IsFull())
				UnscheduleRead();
			else
				base.ScheduleRead();
		}

		return BufferedReadResult::OK;
	}
}

BufferedReadResult
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

	const auto result = TryRead2();

#ifndef NDEBUG
	if (!destructed) {
		assert(reading);
		reading = false;
	} else {
		assert(result == BufferedReadResult::DESTROYED);
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

#ifndef NDEBUG
	DestructObserver destructed{*this};
#endif

	switch (TryRead()) {
	case BufferedReadResult::OK:
	case BufferedReadResult::BLOCKING:
		assert(!destructed);
		assert(IsValid());
		assert(IsConnected());
		break;

	case BufferedReadResult::DISCONNECTED:
		assert(!destructed);
		assert(IsValid());
		assert(!IsConnected());
		return false;

	case BufferedReadResult::DESTROYED:
		assert(destructed || !IsValid());
		return false;
	}

	return true;
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

	write_timeout = Event::Duration(-1);

	handler = nullptr;
	direct = false;
	in_data_handler = false;
	destroyed = false;

#ifndef NDEBUG
	reading = false;
	ended = false;
#endif
}

void
BufferedSocket::Init(SocketDescriptor _fd, FdType _fd_type,
		     Event::Duration _write_timeout,
		     BufferedSocketHandler &_handler) noexcept
{
	assert(!input.IsDefined());

	base.Init(_fd, _fd_type);

	write_timeout = _write_timeout;

	handler = &_handler;
	direct = false;
	in_data_handler = false;
	destroyed = false;

#ifndef NDEBUG
	reading = false;
	ended = false;
#endif
}

void
BufferedSocket::Reinit(Event::Duration _write_timeout,
		       BufferedSocketHandler &_handler) noexcept
{
	assert(IsValid());
	assert(IsConnected());

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

BufferedReadResult
BufferedSocket::Read() noexcept
{
	assert(!reading);
	assert(!destroyed);
	assert(!ended);

	return TryRead();
}

ssize_t
BufferedSocket::Write(std::span<const std::byte> src) noexcept
{
	ssize_t nbytes = base.Write(src);

	if (nbytes < 0) [[unlikely]] {
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

	if (nbytes < 0) [[unlikely]] {
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
BufferedSocket::WriteFrom(FileDescriptor other_fd, FdType other_fd_type,
			  off_t *other_offset,
			  std::size_t length) noexcept
{
	ssize_t nbytes = base.WriteFrom(other_fd, other_fd_type, other_offset,
					length);
	if (nbytes < 0) [[unlikely]] {
		if (const int e = errno; e == EAGAIN) [[likely]] {
			if (!IsReadyForWriting()) {
				ScheduleWrite();
				return WRITE_BLOCKING;
			}

			/* try again, just in case our fd has become ready between
			   the first socket_wrapper_write_from() call and
			   IsReadyForWriting() */
			nbytes = base.WriteFrom(other_fd, other_fd_type,
						other_offset,
						length);
		}
	}

	return nbytes;
}

void
BufferedSocket::DeferRead() noexcept
{
	assert(!ended);
	assert(!destroyed);

	defer_read.Schedule();
}

void
BufferedSocket::ScheduleRead() noexcept
{
	assert(!ended);
	assert(!destroyed);

	if (!in_data_handler && !input.empty())
		/* deferred call to Read() to deliver data from the buffer */
		defer_read.Schedule();
	else
		/* the input buffer is empty: wait for more data from the
		   socket */
		base.ScheduleRead();
}
