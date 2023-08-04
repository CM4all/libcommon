// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Systemd.hxx"
#include "CgroupState.hxx"
#include "lib/dbus/Systemd.hxx"
#include "lib/dbus/Connection.hxx"
#include "lib/dbus/Message.hxx"
#include "lib/dbus/AppendIter.hxx"
#include "lib/dbus/PendingCall.hxx"
#include "lib/dbus/Error.hxx"
#include "lib/dbus/ScopeMatch.hxx"
#include "util/ScopeExit.hxx"
#include "util/StringAPI.hxx"
#include "util/PrintException.hxx"

#include <systemd/sd-daemon.h>

#include <forward_list>

#include <stdlib.h>

CgroupState
CreateSystemdScope(const char *name, const char *description,
		   const SystemdUnitProperties &properties,
		   int pid, bool delegate, const char *slice)
{
	if (!sd_booted())
		return CgroupState();

	ODBus::Error error;

	/* use a private DBus connection and auto-close it, because
	   the spawner will never again need it */
	auto connection = ODBus::Connection::GetSystemPrivate();
	AtScopeExit(&connection) { connection.Close(); };

	const char *match = "type='signal',"
		"sender='org.freedesktop.systemd1',"
		"interface='org.freedesktop.systemd1.Manager',"
		"member='JobRemoved',"
		"path='/org/freedesktop/systemd1'";
	const ODBus::ScopeMatch scope_match(connection, match);

	/* the match for WaitUnitRemoved() */
	const char *unit_removed_match = "type='signal',"
		"sender='org.freedesktop.systemd1',"
		"interface='org.freedesktop.systemd1.Manager',"
		"member='UnitRemoved',"
		"path='/org/freedesktop/systemd1'";
	const ODBus::ScopeMatch unit_removed_scope_match(connection,
							 unit_removed_match);

	using namespace ODBus;

	auto msg = Message::NewMethodCall("org.freedesktop.systemd1",
					  "/org/freedesktop/systemd1",
					  "org.freedesktop.systemd1.Manager",
					  "StartTransientUnit");

	AppendMessageIter args(*msg.Get());
	args.Append(name).Append("replace");

	using PropTypeTraits = StructTypeTraits<StringTypeTraits,
						VariantTypeTraits>;

	const uint32_t pids_value[] = { uint32_t(pid) };

	AppendMessageIter(args, DBUS_TYPE_ARRAY, PropTypeTraits::as_string)
		.Append(Struct(String("Description"),
			       Variant(String(description))))
		.Append(Struct(String("PIDs"),
			       Variant(FixedArray(pids_value, std::size(pids_value)))))
		.Append(Struct(String("Delegate"),
			       Variant(Boolean(delegate))))
		.AppendOptional(slice != nullptr,
				Struct(String("Slice"),
				       Variant(String(slice))))
		.AppendOptional(properties.cpu_weight > 0,
				Struct(String("CPUWeight"),
				       Variant(Uint64(properties.cpu_weight))))
		.AppendOptional(properties.tasks_max > 0,
				Struct(String("TasksMax"),
				       Variant(Uint64(properties.tasks_max))))
		.AppendOptional(properties.memory_min > 0,
				Struct(String("MemoryMin"),
				       Variant(Uint64(properties.memory_min))))
		.AppendOptional(properties.memory_low > 0,
				Struct(String("MemoryLow"),
				       Variant(Uint64(properties.memory_low))))
		.AppendOptional(properties.memory_high > 0,
				Struct(String("MemoryHigh"),
				       Variant(Uint64(properties.memory_high))))
		.AppendOptional(properties.memory_max > 0,
				Struct(String("MemoryMax"),
				       Variant(Uint64(properties.memory_max))))
		.AppendOptional(properties.memory_swap_max > 0,
				Struct(String("MemorySwapMax"),
				       Variant(Uint64(properties.memory_swap_max))))
		.AppendOptional(properties.io_weight > 0,
				Struct(String("IOWeight"),
				       Variant(Uint64(properties.io_weight))))
		.CloseContainer(args);

	using AuxTypeTraits = StructTypeTraits<StringTypeTraits,
					       ArrayTypeTraits<StructTypeTraits<StringTypeTraits,
										VariantTypeTraits>>>;
	args.AppendEmptyArray<AuxTypeTraits>();

	auto pending = PendingCall::SendWithReply(connection, msg.Get());

	dbus_connection_flush(connection);

	pending.Block();

	Message reply = Message::StealReply(*pending.Get());

	/* if the scope already exists, it may be because the previous
	   instance crashed and its spawner process was not yet cleaned up
	   by systemd; try to recover by waiting for the UnitRemoved
	   signal, and then try again to create the scope */
	if (reply.GetType() == DBUS_MESSAGE_TYPE_ERROR &&
	    StringIsEqual(reply.GetErrorName(),
			  "org.freedesktop.systemd1.UnitExists")) {

		/* reset the unit failure state just in case it still
		   exists only because systemd remembers the last
		   failure */
		try {
			Systemd::ResetFailedUnit(connection, name);
		} catch (...) {
			fprintf(stderr, "Failed to reset unit %s: ", name);
			PrintException(std::current_exception());
		}

		if (!Systemd::WaitUnitRemoved(connection, name, 2000)) {
			/* if the old scope is still alive, stop it
			   forcefully; this works around a known
			   problem with LXC and systemd's cgroups1
			   release agent: the agent doesn't get called
			   inside LXC containers, so systemd never
			   cleans up empty units; this is a larger
			   problem affecting everything, but this
			   kludge only solves the infamous spawner
			   failures caused by this */
			fprintf(stderr, "Old unit %s didn't disappear; attempting to stop it\n",
				name);
			try {
				Systemd::StopUnit(connection, name);
				Systemd::WaitUnitRemoved(connection, name, -1);
			} catch (...) {
				fprintf(stderr, "Failed to stop unit %s: ", name);
				PrintException(std::current_exception());
			}
		}

		/* send the StartTransientUnit message again and hope it
		   succeeds this time */
		pending = PendingCall::SendWithReply(connection, msg.Get());
		dbus_connection_flush(connection);
		pending.Block();
		reply = Message::StealReply(*pending.Get());
	}

	reply.CheckThrowError();

	const char *object_path;
	if (!reply.GetArgs(error, DBUS_TYPE_OBJECT_PATH, &object_path))
		error.Throw("StartTransientUnit reply failed");

	Systemd::WaitJobRemoved(connection, object_path);

	return delegate
		? CgroupState::FromProcess()
		: CgroupState();
}
