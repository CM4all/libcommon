/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef ODBUS_CONNECTION_HXX
#define ODBUS_CONNECTION_HXX

#include <dbus/dbus.h>

#include <algorithm>
#include <stdexcept>

namespace ODBus {

/**
 * OO wrapper for a #DBusConnection.
 */
class Connection {
	DBusConnection *c = nullptr;

	explicit Connection(DBusConnection *_c)
		:c(_c) {}

public:
	Connection() = default;

	Connection(const Connection &src)
		:c(dbus_connection_ref(src.c)) {}

	Connection(Connection &&src)
		:c(std::exchange(src.c, nullptr)) {}

	~Connection() {
		if (c != nullptr)
			dbus_connection_unref(c);
	}

	Connection &operator=(Connection &&src) {
		std::swap(c, src.c);
		return *this;
	}

	static Connection GetSystem();

	operator DBusConnection *() {
		return c;
	}

	operator bool() const {
		return c != nullptr;
	}
};

} /* namespace ODBus */

#endif
