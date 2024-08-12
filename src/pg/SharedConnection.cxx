// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "SharedConnection.hxx"

namespace Pg {

void
SharedConnection::ScheduleQuery(SharedConnectionQuery &query) noexcept
{
	assert(!query.IsScheduled());

	const bool was_empty = queries.empty();
	queries.push_back(query);

	/* connect if we're not already connected, or reconnect really
	   soon if reconnect is pending (skip the reconnect delay) */
	connection.MaybeScheduleConnect();

	if (was_empty && connection.IsDefined() && connection.IsIdle() &&
	    !submitted)
		defer_submit_next.Schedule();
}

void
SharedConnection::CancelQuery(SharedConnectionQuery &query) noexcept
{
	assert(!queries.empty());
	assert(query.IsScheduled());

	const bool was_submitted = WasSubmitted(query);
	submitted = false;

	queries.erase(queries.iterator_to(query));

	if (was_submitted) {
		/* if the query currently "owns" the connection, it is
		   usually not idle, but maybe it's waiting for
		   something else inbetween two queries, so we need to
		   check anyway */
		if (connection.IsRequestPending())
			connection.RequestCancel();

		/* submit the next query (outside of this caller
		   chain, using the DeferEvent) */
		if (!queries.empty())
			defer_submit_next.Schedule();
	}
}

void
SharedConnection::SubmitNext() noexcept
{
	assert(connection.IsDefined());
	assert(connection.IsIdle());
	assert(!submitted);
	assert(!queries.empty());

	defer_submit_next.Cancel();

	auto &query = queries.front();

	try {
		submitted = true;

		query.OnPgConnectionAvailable(connection);
	} catch (...) {
		assert(submitted);
		assert(!queries.empty());
		assert(&queries.front() == &query);

		submitted = false;

		queries.erase(queries.iterator_to(query));
		query.OnPgError(std::current_exception());

		if (!queries.empty() && connection.IsIdle())
			/* this one failed for some reason, but the
			   connection is still alive - submit the next
			   one */
			defer_submit_next.Schedule();
	}
}

void
SharedConnection::OnConnect()
{
	assert(connection.IsDefined());
	assert(connection.IsIdle());
	assert(!defer_submit_next.IsPending());
	assert(!submitted);

	handler.OnPgConnect();

	if (!queries.empty())
		SubmitNext();
}

void
SharedConnection::OnIdle()
{
	assert(connection.IsDefined());
	assert(connection.IsIdle());
}

void
SharedConnection::OnDisconnect() noexcept
{
	defer_submit_next.Cancel();

	if (submitted) {
		/* just in case the current query hasn't cancelled
		   itself yet */
		// TODO convert to assert(!submitted)
		assert(!queries.empty());
		queries.pop_front();
		submitted = false;
	}
}

void
SharedConnection::OnNotify(const char *name)
{
	handler.OnPgNotify(name);
}

void
SharedConnection::OnError(std::exception_ptr e) noexcept
{
	defer_submit_next.Cancel();

	if (submitted) {
		/* the query was already submitted, thus we don't need
		   to call SharedConnectionQuery::OnPgError(); the
		   class will receive error information via
		   AsyncResultHandler */
		assert(!queries.empty());
		queries.pop_front();
		submitted = false;
	} else if (!queries.empty()) {
		/* the query was not yet submitted; abort it */
		auto &query = queries.pop_front();
		query.OnPgError(e);

		/* note that we don't cancel more queries that may
		   still be queued; AsyncConnection will now schedule
		   a reconnect timer and the other queries will run
		   later */
	}

	handler.OnPgError(std::move(e));
}

} /* namespace Pg */
