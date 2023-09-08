// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "AsyncConnection.hxx"
#include "event/DeferEvent.hxx"
#include "util/IntrusiveList.hxx"

namespace Pg {

/**
 * Handler class for #SharedConnection.
 */
class SharedConnectionHandler {
public:
	virtual void OnPgConnect() {
	}

	virtual void OnPgNotify(const char *name) {
		(void)name;
	}

	/**
	 * An error has occurred (may be fatal or not), and the
	 * handler can implement this method to log the error
	 * condition.
	 *
	 * If this was a fatal error which closed previously
	 * successful connection, then OnDisconnect() will be called
	 * right after this method.
	 */
	virtual void OnPgError(std::exception_ptr e) noexcept = 0;
};

class SharedConnection;

/**
 * A query (or multiple queries); derive from this class and pass it
 * to SharedConnection::ScheduleQuery().  As soon as the connection
 * becomes available, OnPgConnectionAvailable() is invoked, or
 * OnPgError().  This class may then send queries and must call
 * Cancel() to release the connection.
 */
class SharedConnectionQuery {
	friend class SharedConnection;
	IntrusiveListHook<IntrusiveHookMode::TRACK> shared_connection_query_siblings;

	SharedConnection &shared_connection;

public:
	explicit SharedConnectionQuery(SharedConnection &_shared_connection) noexcept
		:shared_connection(_shared_connection) {}

	~SharedConnectionQuery() noexcept {
		Cancel();
	}

	bool IsScheduled() const noexcept {
		return shared_connection_query_siblings.is_linked();
	}

	void Cancel() noexcept;

	/**
	 * The connection has become available.  This method may
	 * submit the actual query to the #AsyncConnection and wait
	 * for results.  Call Cancel() (or destruct this object) when
	 * you're done.
	 *
	 * Exceptions thrown by this method will be passed to
	 * OnPgError().
	 */
	virtual void OnPgConnectionAvailable(AsyncConnection &connection) = 0;

	virtual void OnPgError(std::exception_ptr error) noexcept = 0;
};

/**
 * An extension of #AsyncConnection which manages a queue of queries
 * to be submitted.
 */
class SharedConnection : public AsyncConnectionHandler {
	AsyncConnection connection;

	DeferEvent defer_submit_next;

	SharedConnectionHandler &handler;

	IntrusiveList<SharedConnectionQuery,
		      IntrusiveListMemberHookTraits<&SharedConnectionQuery::shared_connection_query_siblings>> queries;

	bool submitted = false;

public:
	SharedConnection(EventLoop &event_loop,
			 const char *conninfo, const char *schema,
			 SharedConnectionHandler &_handler) noexcept
		:connection(event_loop, conninfo, schema, *this),
		 defer_submit_next(event_loop, BIND_THIS_METHOD(SubmitNext)),
		 handler(_handler)
	{
	}

	auto &GetEventLoop() const noexcept {
		return connection.GetEventLoop();
	}

	void ScheduleQuery(SharedConnectionQuery &query) noexcept;
	void CancelQuery(SharedConnectionQuery &query) noexcept;

private:
	bool WasSubmitted(const SharedConnectionQuery &query) const noexcept {
		return submitted && &query == &queries.front();
	}

	void SubmitNext() noexcept;

	/* virtual methods from class Pg::AsyncConnectionHandler */
	void OnConnect() override;
	void OnIdle() override;
	void OnDisconnect() noexcept override;
	void OnNotify(const char *name) override;
	void OnError(std::exception_ptr e) noexcept override;
};

inline void
SharedConnectionQuery::Cancel() noexcept
{
	if (IsScheduled())
		shared_connection.CancelQuery(*this);
}

} /* namespace Pg */
