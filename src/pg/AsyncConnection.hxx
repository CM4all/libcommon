/*
 * Copyright 2007-2020 CM4all GmbH
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

#pragma once

#include "Connection.hxx"
#include "event/SocketEvent.hxx"
#include "event/TimerEvent.hxx"

#include <cassert>

namespace Pg {

class AsyncConnectionHandler {
public:
	/**
	 * A connection has been established successfully, and the
	 * connection is ready for queries.
	 *
	 * Exceptions thrown by this method will be reported to
	 * OnError(), and the connection will be closed.
	 */
	virtual void OnConnect() = 0;

	/**
	 * Called when the connection becomes idle, i.e. ready for a
	 * query after the previous query result was finished.  It is
	 * not called when the connection becomes idle for the first
	 * time after the connection has been established; in that
	 * case, only OnConnect() is called.
	 *
	 * Exceptions thrown by this method will be reported to
	 * OnError(), and the connection will be closed.
	 */
	virtual void OnIdle() {}

	/**
         * The database connection was closed due to a fatal error.
         * This method does not get called when
         * AsyncConnectionHandler::Disconnect() gets called, and it
         * also doesn't get called when a (re)connection attempt
         * fails.
	 */
	virtual void OnDisconnect() noexcept = 0;

	/**
	 * Exceptions thrown by this method will be reported to
	 * OnError(), and the connection will be closed.
	 */
	virtual void OnNotify(const char *name) = 0;

	/**
	 * An error has occurred (may be fatal or not), and the
	 * handler can implement this method to log the error
	 * condition.
	 *
	 * If this was a fatal error which closed previously
	 * successful connection, the OnDisconnect() will be called
	 * right after this method.
	 */
	virtual void OnError(std::exception_ptr e) noexcept = 0;
};

class AsyncResultHandler {
public:
	/**
	 * Exceptions thrown by this method will be reported to
	 * AsyncConnectionHandler::OnError(), and the connection will
	 * be closed.
	 */
	virtual void OnResult(Result &&result) = 0;

	/**
	 * Exceptions thrown by this method will be reported to
	 * AsyncConnectionHandler::OnError(), and the connection will
	 * be closed.
	 */
	virtual void OnResultEnd() = 0;

	virtual void OnResultError() noexcept {
		OnResultEnd();
	}
};

/**
 * A PostgreSQL database connection that connects asynchronously,
 * reconnects automatically and provides an asynchronous notify
 * handler.
 */
class AsyncConnection : public Connection {
	const std::string conninfo;
	const std::string schema;

	AsyncConnectionHandler &handler;

	enum class State {
		/**
		 * No database connection exists.
		 *
		 * In this state, the connect may have failed
		 * (IsDefined() returns true) or the connect was never
		 * attempted (IsDefined() returns false).
		 *
		 * The #reconnect_timer may be configured to retry
		 * connecting somewhat later.
		 */
		DISCONNECTED,

		/**
		 * Connecting to the database asynchronously.
		 */
		CONNECTING,

		/**
		 * Reconnecting to the database asynchronously.
		 */
		RECONNECTING,

		/**
		 * Connection is ready to be used.  As soon as the socket
		 * becomes readable, notifications will be received and
		 * forwarded to AsyncConnectionHandler::OnNotify().
		 */
		READY,
	};

	State state = State::DISCONNECTED;

	/**
	 * DISCONNECTED: not used.
	 *
	 * CONNECTING: used by PollConnect().
	 *
	 * RECONNECTING: used by PollReconnect().
	 *
	 * READY: used by PollNotify().
	 */
	SocketEvent socket_event;

	/**
	 * A timer which reconnects during State::DISCONNECTED.
	 */
	TimerEvent reconnect_timer;

	AsyncResultHandler *result_handler = nullptr;

public:
	/**
	 * Construct the object, but do not initiate the connect yet.
	 * Call Connect() to do that.
	 */
	AsyncConnection(EventLoop &event_loop,
			const char *conninfo, const char *schema,
			AsyncConnectionHandler &handler) noexcept;

	auto &GetEventLoop() const noexcept {
		return socket_event.GetEventLoop();
	}

	const std::string &GetSchemaName() const noexcept {
		return schema;
	}

	bool IsReady() const noexcept {
		return state == State::READY;
	}

	/**
	 * Initiate the initial connect.  This may be called only once.
	 */
	void Connect() noexcept;

	void Reconnect() noexcept;

	void Disconnect() noexcept;

	/**
	 * Returns true when no asynchronous query is in progress.  In
	 * this case, SendQuery() may be called.
	 */
	gcc_pure
	bool IsIdle() const {
		assert(IsDefined());

		return state == State::READY && result_handler == nullptr;
	}

	/**
	 * Call this after catching a fatal connection error.  This
	 * will close the connection, notify the handler and schedule
	 * a reconnect.
	 */
	void Error(std::exception_ptr e) noexcept;

	/**
	 * Call this after catching an exception.  If it is a "fatal"
	 * #Pg::Error, it will call Error(); otherwise, it will only
	 * forward the exception on AsyncConnectionHandler::OnError().
	 *
	 * @return true if the error was fatal
	 */
	bool CheckError(std::exception_ptr e) noexcept;

	template<typename... Params>
	void SendQuery(AsyncResultHandler &_handler, Params... params) {
		assert(IsIdle());

		result_handler = &_handler;

		try {
			Connection::SendQuery(params...);
		} catch (...) {
			result_handler = nullptr;
			throw;
		}
	}

	void CheckNotify() noexcept {
		if (IsReady())
			PollNotify();
	}

protected:
	/**
	 * This method is called when an fatal error on the connection has
	 * occurred.  It will set the state to DISCONNECTED, notify the
	 * handler, and schedule a reconnect.
	 */
	void Error() noexcept;

	void Poll(PostgresPollingStatusType status) noexcept;

	void PollConnect() noexcept;
	void PollReconnect() noexcept;
	void PollResult();
	void PollNotify() noexcept;

	void ScheduleReconnect() noexcept;

private:
	void OnSocketEvent(unsigned events) noexcept;
	void OnReconnectTimer() noexcept;
};

} /* namespace Pg */
