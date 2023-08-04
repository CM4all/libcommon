// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

namespace ODBus {
class Connection;
}

namespace Systemd {

/**
 * Throws on error.
 */
void
WaitJobRemoved(ODBus::Connection &connection, const char *object_path);

/**
 * Wait for the UnitRemoved signal for the specified unit name.
 */
bool
WaitUnitRemoved(ODBus::Connection &connection, const char *name,
		int timeout_ms) noexcept;

/**
 * Note: the caller must establish a match on "JobRemoved".
 *
 * Throws on error.
 */
void
StopUnit(ODBus::Connection &connection,
	    const char *name, const char *mode="replace");

/**
 * Resets the "failed" state of a specific unit.
 *
 * Throws on error.
 */
void
ResetFailedUnit(ODBus::Connection &connection, const char *name);

}
