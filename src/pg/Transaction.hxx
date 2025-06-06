// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "Connection.hxx"
#include "Error.hxx"

#include <concepts>

namespace Pg {

/**
 * Like Connection::DoSerializable(), but retry on
 * "serialization_failure" (Error::IsSerializationFailure()).
 */
void
DoSerializableRepeat(Pg::Connection &connection, unsigned retries,
		     std::invocable<> auto f)
{
	while (true) {
		try {
			connection.DoSerializable(f);
			break;
		} catch (const Pg::Error &e) {
			if (e.IsSerializationFailure() &&
			    retries-- > 0)
				/* try again */
				continue;

			throw;
		}
	}
}

/**
 * Like Connection::DoRepeatableRead(), but retry on
 * "serialization_failure" (Error::IsSerializationFailure()).
 */
void
DoRepeatableReadRepeat(Pg::Connection &connection, unsigned retries,
		       std::invocable<> auto f)
{
	while (true) {
		try {
			connection.DoRepeatableRead(f);
			break;
		} catch (const Pg::Error &e) {
			if (e.IsSerializationFailure() &&
			    retries-- > 0)
				/* try again */
				continue;

			throw;
		}
	}
}

} /* namespace Pg */
