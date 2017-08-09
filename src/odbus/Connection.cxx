/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Connection.hxx"
#include "Error.hxx"

ODBus::Connection
ODBus::Connection::GetSystem()
{
	ODBus::Error error;
	auto *c = dbus_bus_get(DBUS_BUS_SYSTEM, error);
	error.CheckThrow("DBus connection error");
	return Connection(c);
}
