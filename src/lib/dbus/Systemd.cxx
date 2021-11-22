/*
 * Copyright 2007-2021 CM4all GmbH
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

#include "Systemd.hxx"
#include "Connection.hxx"
#include "Message.hxx"
#include "AppendIter.hxx"
#include "PendingCall.hxx"
#include "Error.hxx"

#include <string.h>

namespace Systemd {

void
WaitJobRemoved(ODBus::Connection &connection, const char *object_path)
{
	using namespace ODBus;

	while (true) {
		auto msg = Message::Pop(*connection);
		if (!msg.IsDefined()) {
			if (dbus_connection_read_write(connection, -1))
				continue;
			else
				break;
		}

		if (msg.IsSignal("org.freedesktop.systemd1.Manager", "JobRemoved")) {
			Error error;
			dbus_uint32_t job_id;
			const char *removed_object_path, *unit_name, *result_string;
			if (!msg.GetArgs(error,
					 DBUS_TYPE_UINT32, &job_id,
					 DBUS_TYPE_OBJECT_PATH, &removed_object_path,
					 DBUS_TYPE_STRING, &unit_name,
					 DBUS_TYPE_STRING, &result_string))
				error.Throw("JobRemoved failed");

			if (strcmp(removed_object_path, object_path) == 0)
				break;
		}
	}
}

bool
WaitUnitRemoved(ODBus::Connection &connection, const char *name,
		int timeout_ms) noexcept
{
	using namespace ODBus;

	bool was_empty = false;

	while (true) {
		auto msg = Message::Pop(*connection);
		if (!msg.IsDefined()) {
			if (was_empty)
				return false;

			was_empty = true;

			if (dbus_connection_read_write(connection, timeout_ms))
				continue;
			else
				return false;
		}

		if (msg.IsSignal("org.freedesktop.systemd1.Manager", "UnitRemoved")) {
			DBusError err;
			dbus_error_init(&err);

			const char *unit_name, *object_path;
			if (!msg.GetArgs(err,
					 DBUS_TYPE_STRING, &unit_name,
					 DBUS_TYPE_OBJECT_PATH, &object_path)) {
				dbus_error_free(&err);
				return false;
			}

			if (strcmp(unit_name, name) == 0)
				return true;
		}
	}
}

void
StopService(ODBus::Connection &connection,
	    const char *name, const char *mode)
{
	using namespace ODBus;

	auto msg = Message::NewMethodCall("org.freedesktop.systemd1",
					  "/org/freedesktop/systemd1",
					  "org.freedesktop.systemd1.Manager",
					  "StopUnit");

	AppendMessageIter(*msg.Get()).Append(name).Append(mode);

	auto pending = PendingCall::SendWithReply(connection, msg.Get());

	dbus_connection_flush(connection);

	pending.Block();

	Message reply = Message::StealReply(*pending.Get());
	reply.CheckThrowError();

	Error error;
	const char *object_path;
	if (!reply.GetArgs(error, DBUS_TYPE_OBJECT_PATH, &object_path))
		error.Throw("StopUnit reply failed");

	WaitJobRemoved(connection, object_path);
}

void
ResetFailedUnit(ODBus::Connection &connection, const char *name)
{
	using namespace ODBus;

	auto msg = Message::NewMethodCall("org.freedesktop.systemd1",
					  "/org/freedesktop/systemd1",
					  "org.freedesktop.systemd1.Manager",
					  "ResetFailedUnit");

	AppendMessageIter(*msg.Get()).Append(name);

	auto pending = PendingCall::SendWithReply(connection, msg.Get());

	dbus_connection_flush(connection);

	pending.Block();

	Message reply = Message::StealReply(*pending.Get());
	reply.CheckThrowError();
}

} // namespace Systemd
