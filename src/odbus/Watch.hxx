/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef ODBUS_WATCH_HXX
#define ODBUS_WATCH_HXX

#include "Connection.hxx"
#include "event/SocketEvent.hxx"
#include "event/DeferEvent.hxx"
#include "util/Compiler.h"

#include <dbus/dbus.h>

#include <map>

class EventLoop;

namespace ODBus {

/**
 * Integrate a DBusConnection into the #EventLoop.
 */
class WatchManager {
	Connection connection;

	class Watch {
		WatchManager &parent;
		DBusWatch &watch;
		SocketEvent event;

	public:
		Watch(EventLoop &event_loop, WatchManager &_parent,
		      DBusWatch &_watch);

		~Watch() {
			event.Delete();
		}

		void Toggled();

	private:
		void OnSocketReady(unsigned events);
	};

	std::map<DBusWatch *, Watch> watches;

	DeferEvent defer_dispatch;

public:
	template<typename C>
	WatchManager(EventLoop &event_loop, C &&_connection)
		:connection(std::forward<C>(_connection)),
		 defer_dispatch(event_loop, BIND_THIS_METHOD(Dispatch))
	{
		dbus_connection_set_watch_functions(connection,
						    AddFunction,
						    RemoveFunction,
						    ToggledFunction,
						    (void *)this,
						    nullptr);
	}

	~WatchManager() {
		Shutdown();
	}

	WatchManager(const WatchManager &) = delete;
	WatchManager &operator=(const WatchManager &) = delete;

	void Shutdown();

	EventLoop &GetEventLoop() {
		return defer_dispatch.GetEventLoop();
	}

	void ScheduleDispatch() {
		defer_dispatch.Schedule();
	}

private:
	void Dispatch();

	bool Add(DBusWatch *watch);
	void Remove(DBusWatch *watch);
	void Toggled(DBusWatch *watch);

	static dbus_bool_t AddFunction(DBusWatch *watch, void *data) {
		auto &wm = *(WatchManager *)data;
		return wm.Add(watch);
	}

	static void RemoveFunction(DBusWatch *watch, void *data) {
		auto &wm = *(WatchManager *)data;
		wm.Remove(watch);
	}

	static void ToggledFunction(DBusWatch *watch, void *data) {
		auto &wm = *(WatchManager *)data;
		wm.Toggled(watch);
	}
};

} /* namespace ODBus */

#endif
