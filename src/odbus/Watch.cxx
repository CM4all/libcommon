/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Watch.hxx"

namespace ODBus {

WatchManager::Watch::Watch(EventLoop &event_loop,
			   WatchManager &_parent, DBusWatch &_watch)
	:parent(_parent), watch(_watch),
	 event(event_loop, -1, 0, BIND_THIS_METHOD(OnSocketReady))
{
	Toggled();
}

static constexpr unsigned
DbusToLibevent(unsigned flags)
{
	return ((flags & DBUS_WATCH_READABLE) != 0) * SocketEvent::READ |
		((flags & DBUS_WATCH_WRITABLE) != 0) * SocketEvent::WRITE;
}

void
WatchManager::Watch::Toggled()
{
	event.Delete();

	if (dbus_watch_get_enabled(&watch)) {
		event.Set(dbus_watch_get_unix_fd(&watch),
			  SocketEvent::PERSIST | DbusToLibevent(dbus_watch_get_flags(&watch)));
		event.Add();
	}
}

static constexpr unsigned
LibeventToDbus(unsigned flags)
{
	return ((flags & SocketEvent::READ) != 0) * DBUS_WATCH_READABLE |
		((flags & SocketEvent::WRITE) != 0) * DBUS_WATCH_WRITABLE;
}

void
WatchManager::Watch::OnSocketReady(unsigned events)
{
	dbus_watch_handle(&watch, LibeventToDbus(events));
	parent.ScheduleDispatch();
}

void
WatchManager::Shutdown()
{
	dbus_connection_set_watch_functions(connection,
					    nullptr, nullptr,
					    nullptr, nullptr,
					    nullptr);
	watches.clear();
	defer_dispatch.Cancel();
}

void
WatchManager::Dispatch()
{
	while (dbus_connection_dispatch(connection) == DBUS_DISPATCH_DATA_REMAINS) {}
}

bool
WatchManager::Add(DBusWatch *watch)
{
	watches.emplace(std::piecewise_construct,
			std::forward_as_tuple(watch),
			std::forward_as_tuple(GetEventLoop(), *this, *watch));
	return true;
}

void
WatchManager::Remove(DBusWatch *watch)
{
	watches.erase(watch);
}

void
WatchManager::Toggled(DBusWatch *watch)
{
	auto i = watches.find(watch);
	assert(i != watches.end());

	i->second.Toggled();
}

} /* namespace ODBus */
