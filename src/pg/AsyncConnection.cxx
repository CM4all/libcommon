/*
 * Copyright 2007-2018 Content Management AG
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

#include "AsyncConnection.hxx"
#include "util/Exception.hxx"

namespace Pg {

AsyncConnection::AsyncConnection(EventLoop &event_loop,
				 const char *_conninfo, const char *_schema,
				 AsyncConnectionHandler &_handler) noexcept
	:conninfo(_conninfo), schema(_schema),
	 handler(_handler),
	 socket_event(event_loop, BIND_THIS_METHOD(OnSocketEvent)),
	 reconnect_timer(event_loop, BIND_THIS_METHOD(OnReconnectTimer))
{
}

void
AsyncConnection::Error() noexcept
{
	assert(state == State::CONNECTING ||
	       state == State::RECONNECTING ||
	       state == State::READY);

	socket_event.Cancel();

	const bool was_connected = state == State::READY;
	state = State::DISCONNECTED;

	if (result_handler != nullptr) {
		auto rh = result_handler;
		result_handler = nullptr;
		rh->OnResultError();
	}

	if (was_connected)
		handler.OnDisconnect();

	ScheduleReconnect();
}

void
AsyncConnection::Poll(PostgresPollingStatusType status) noexcept
{
	switch (status) {
	case PGRES_POLLING_FAILED:
		handler.OnError(std::make_exception_ptr(std::runtime_error(GetErrorMessage())));
		Error();
		break;

	case PGRES_POLLING_READING:
		socket_event.Open(SocketDescriptor(GetSocket()));
		socket_event.ScheduleRead();
		break;

	case PGRES_POLLING_WRITING:
		socket_event.Open(SocketDescriptor(GetSocket()));
		socket_event.ScheduleWrite();
		break;

	case PGRES_POLLING_OK:
		if (!schema.empty() &&
		    (state == State::CONNECTING || state == State::RECONNECTING)) {
			try {
				SetSchema(schema.c_str());
			} catch (...) {
				handler.OnError(NestCurrentException(std::runtime_error("Failed to set schema")));

				Error();
				break;
			}
		}

		state = State::READY;
		socket_event.Open(SocketDescriptor(GetSocket()));
		socket_event.ScheduleRead();

		try {
			handler.OnConnect();
		} catch (...) {
			handler.OnError(std::current_exception());
			Error();
			break;
		}

		/* check the connection status, just in case the handler
		   method has done awful things */
		if (state == State::READY &&
		    GetStatus() == CONNECTION_BAD)
			Error();
		break;

	case PGRES_POLLING_ACTIVE:
		/* deprecated enum value */
		assert(false);
		break;
	}
}

void
AsyncConnection::PollConnect() noexcept
{
	assert(IsDefined());
	assert(state == State::CONNECTING);

	Poll(Connection::PollConnect());
}

void
AsyncConnection::PollReconnect() noexcept
{
	assert(IsDefined());
	assert(state == State::RECONNECTING);

	Poll(Connection::PollReconnect());
}

inline void
AsyncConnection::PollResult()
{
	while (!IsBusy()) {
		auto result = ReceiveResult();
		if (result_handler != nullptr) {
			if (result.IsDefined())
				result_handler->OnResult(std::move(result));
			else {
				auto rh = result_handler;
				result_handler = nullptr;
				rh->OnResultEnd();
			}
		}

		if (!result.IsDefined())
			break;
	}
}

void
AsyncConnection::PollNotify() noexcept
{
	assert(IsDefined());
	assert(state == State::READY);

	const bool was_idle = IsIdle();

	ConsumeInput();

	Notify notify;
	switch (GetStatus()) {
	case CONNECTION_OK:
		try {
			PollResult();

			while ((notify = GetNextNotify()))
				handler.OnNotify(notify->relname);

			if (!was_idle && IsIdle())
				handler.OnIdle();
		} catch (...) {
			handler.OnError(std::current_exception());
			Error();
		}

		break;

	case CONNECTION_BAD:
		Error();
		break;

	default:
		break;
	}
}

void
AsyncConnection::Connect() noexcept
{
	assert(!IsDefined());
	assert(state == State::DISCONNECTED);

	state = State::CONNECTING;

	try {
		StartConnect(conninfo.c_str());
	} catch (...) {
		Connection::Disconnect();
		handler.OnError(std::current_exception());
		Error();
		return;
	}

	PollConnect();
}

void
AsyncConnection::Reconnect() noexcept
{
	assert(IsDefined());

	socket_event.Cancel();
	StartReconnect();
	state = State::RECONNECTING;
	PollReconnect();
}

void
AsyncConnection::Disconnect() noexcept
{
	if (!IsDefined())
		return;

	socket_event.Cancel();
	reconnect_timer.Cancel();
	Connection::Disconnect();
	state = State::DISCONNECTED;
}

void
AsyncConnection::ScheduleReconnect() noexcept
{
	/* attempt to reconnect every 10 seconds */
	static constexpr struct timeval delay{ 10, 0 };

	assert(state == State::DISCONNECTED);

	reconnect_timer.Add(delay);
}

inline void
AsyncConnection::OnSocketEvent(unsigned) noexcept
{
	switch (state) {
	case State::DISCONNECTED:
		assert(false);
		gcc_unreachable();

	case State::CONNECTING:
		socket_event.Cancel();
		PollConnect();
		break;

	case State::RECONNECTING:
		socket_event.Cancel();
		PollReconnect();
		break;

	case State::READY:
		PollNotify();
		break;
	}
}

inline void
AsyncConnection::OnReconnectTimer() noexcept
{
	assert(state == State::DISCONNECTED);

	if (!IsDefined())
		/* there was never a socket, i.e. StartConnect() has failed
		   (maybe due to a DNS failure) - retry that method */
		Connect();
	else
		Reconnect();
}

} /* namespace Pg */
