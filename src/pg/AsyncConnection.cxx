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

#include "AsyncConnection.hxx"

namespace Pg {

AsyncConnection::AsyncConnection(EventLoop &event_loop,
				 const char *_conninfo, const char *_schema,
				 AsyncConnectionHandler &_handler)
	:conninfo(_conninfo), schema(_schema),
	 handler(_handler),
	 socket_event(event_loop, -1, 0, BIND_THIS_METHOD(OnSocketEvent)),
	 reconnect_timer(event_loop, BIND_THIS_METHOD(OnReconnectTimer))
{
}

void
AsyncConnection::Error()
{
	assert(state == State::CONNECTING ||
	       state == State::RECONNECTING ||
	       state == State::READY);

	socket_event.Delete();

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
AsyncConnection::Poll(PostgresPollingStatusType status)
{
	switch (status) {
	case PGRES_POLLING_FAILED:
		handler.OnError("Failed to connect to database", GetErrorMessage());
		Error();
		break;

	case PGRES_POLLING_READING:
		socket_event.Set(GetSocket(), SocketEvent::READ);
		socket_event.Add();
		break;

	case PGRES_POLLING_WRITING:
		socket_event.Set(GetSocket(), SocketEvent::WRITE);
		socket_event.Add();
		break;

	case PGRES_POLLING_OK:
		if (!schema.empty() &&
		    (state == State::CONNECTING || state == State::RECONNECTING) &&
		    !SetSchema(schema.c_str())) {
			handler.OnError("Failed to set schema", GetErrorMessage());
			Error();
			break;
		}

		state = State::READY;
		socket_event.Set(GetSocket(), SocketEvent::READ|SocketEvent::PERSIST);
		socket_event.Add();

		handler.OnConnect();

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
AsyncConnection::PollConnect()
{
	assert(IsDefined());
	assert(state == State::CONNECTING);

	Poll(Connection::PollConnect());
}

void
AsyncConnection::PollReconnect()
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
AsyncConnection::PollNotify()
{
	assert(IsDefined());
	assert(state == State::READY);

	const bool was_idle = IsIdle();

	ConsumeInput();

	Notify notify;
	switch (GetStatus()) {
	case CONNECTION_OK:
		PollResult();

		while ((notify = GetNextNotify()))
			handler.OnNotify(notify->relname);

		if (!was_idle && IsIdle())
			handler.OnIdle();
		break;

	case CONNECTION_BAD:
		Error();
		break;

	default:
		break;
	}
}

void
AsyncConnection::Connect()
{
	assert(state == State::UNINITIALIZED || state == State::WAITING);

	state = State::CONNECTING;

	try {
		StartConnect(conninfo.c_str());
	} catch (const std::runtime_error &e) {
		Connection::Disconnect();
		handler.OnError("Failed to connect to database", e.what());
		Error();
		return;
	}

	PollConnect();
}

void
AsyncConnection::Reconnect()
{
	assert(state != State::UNINITIALIZED);

	socket_event.Delete();
	StartReconnect();
	state = State::RECONNECTING;
	PollReconnect();
}

void
AsyncConnection::Disconnect()
{
	if (state == State::UNINITIALIZED)
		return;

	socket_event.Delete();
	reconnect_timer.Cancel();
	Connection::Disconnect();
	state = State::DISCONNECTED;
}

void
AsyncConnection::ScheduleReconnect()
{
	/* attempt to reconnect every 10 seconds */
	static constexpr struct timeval delay{ 10, 0 };

	assert(state == State::DISCONNECTED);

	state = State::WAITING;
	reconnect_timer.Add(delay);
}

inline void
AsyncConnection::OnSocketEvent(unsigned)
{
	switch (state) {
	case State::UNINITIALIZED:
	case State::DISCONNECTED:
	case State::WAITING:
		assert(false);
		gcc_unreachable();

	case State::CONNECTING:
		PollConnect();
		break;

	case State::RECONNECTING:
		PollReconnect();
		break;

	case State::READY:
		PollNotify();
		break;
	}
}

inline void
AsyncConnection::OnReconnectTimer()
{
	assert(state == State::WAITING);

	if (socket_event.GetFd() < 0)
		/* there was never a socket, i.e. StartConnect() has failed
		   (maybe due to a DNS failure) - retry that method */
		Connect();
	else
		Reconnect();
}

} /* namespace Pg */
