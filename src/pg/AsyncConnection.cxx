// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "AsyncConnection.hxx"
#include "Error.hxx"
#include "util/Compiler.h"
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

	socket_event.Abandon();

	const bool was_connected = state == State::READY;
	state = State::DISCONNECTED;

	if (result_handler != nullptr) {
		auto rh = result_handler;
		result_handler = nullptr;
		rh->OnResultError();
	}

	cancelling = false;

	if (was_connected)
		handler.OnDisconnect();

	ScheduleReconnect();
}

void
AsyncConnection::Error(std::exception_ptr e) noexcept
{
	handler.OnError(std::move(e));

	if (state != State::DISCONNECTED)
		/* invoke Error() only if state==READY to allow
		   calling this method without triggering an assertion
		   failure in Error() */
		Error();
}

[[gnu::pure]]
static bool
IsFatalPgError(std::exception_ptr ep) noexcept
{
	try {
		FindRetrowNested<Pg::Error>(ep);
		return false;
	} catch (const Pg::Error &e) {
		return e.IsFatal();
	} catch (...) {
		return false;
	}
}

bool
AsyncConnection::CheckError(std::exception_ptr e) noexcept
{
	const bool fatal = IsFatalPgError(e);
	if (fatal)
		Error(std::move(e));
	else
		handler.OnError(std::move(e));
	return fatal;
}

void
AsyncConnection::Poll(PostgresPollingStatusType status) noexcept
try {
	switch (status) {
	case PGRES_POLLING_FAILED:
		throw std::runtime_error(GetErrorMessage());

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
				std::throw_with_nested(std::runtime_error("Failed to set schema"));
			}
		}

		state = State::READY;
		socket_event.Open(SocketDescriptor(GetSocket()));
		socket_event.ScheduleRead();

		handler.OnConnect(); /* may throw */

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
} catch (...) {
	Error(std::current_exception());
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
		const bool had_result = result.IsDefined();
		if (result_handler != nullptr) {
			if (result.IsDefined())
				result_handler->OnResult(std::move(result));
			else {
				auto rh = result_handler;
				result_handler = nullptr;
				rh->OnResultEnd();
			}
		} else if (cancelling) {
			/* discard results of cancelled queries */
			if (!result.IsDefined())
				cancelling = false;
		}

		if (!had_result)
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
			Error(std::current_exception());
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

	reconnect_timer.Cancel();
	state = State::CONNECTING;

	try {
		StartConnect(conninfo.c_str());
	} catch (...) {
		Connection::Disconnect();
		Error(std::current_exception());
		return;
	}

	PollConnect();
}

void
AsyncConnection::Reconnect() noexcept
{
	assert(IsDefined());

	reconnect_timer.Cancel();
	socket_event.ReleaseSocket();
	StartReconnect();
	state = State::RECONNECTING;
	PollReconnect();
}

void
AsyncConnection::Disconnect() noexcept
{
	reconnect_timer.Cancel();

	if (!IsDefined())
		return;

	socket_event.Abandon();
	Connection::Disconnect();
	state = State::DISCONNECTED;
}

void
AsyncConnection::ScheduleReconnect() noexcept
{
	assert(state == State::DISCONNECTED);

	/* attempt to reconnect every 10 seconds */
	reconnect_timer.Schedule(std::chrono::seconds(10));
}

inline void
AsyncConnection::OnSocketEvent(unsigned) noexcept
{
	switch (state) {
	case State::DISCONNECTED:
		assert(false);
		gcc_unreachable();

	case State::CONNECTING:
		socket_event.ReleaseSocket();
		PollConnect();
		break;

	case State::RECONNECTING:
		socket_event.ReleaseSocket();
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
