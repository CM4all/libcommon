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
#include "CgroupState.hxx"
#include "odbus/Systemd.hxx"
#include "odbus/Connection.hxx"
#include "odbus/Message.hxx"
#include "odbus/AppendIter.hxx"
#include "odbus/PendingCall.hxx"
#include "odbus/Error.hxx"
#include "odbus/ScopeMatch.hxx"
#include "util/IterableSplitString.hxx"
#include "util/ScopeExit.hxx"
#include "util/StringCompare.hxx"
#include "util/PrintException.hxx"

#include <systemd/sd-daemon.h>

#include <forward_list>

#include <stdlib.h>
#include <stdio.h>

static FILE *
OpenProcCgroup(unsigned pid)
{
	if (pid > 0) {
		char buffer[256];
		sprintf(buffer, "/proc/%u/cgroup", pid);
		return fopen(buffer, "r");
	} else
		return fopen("/proc/self/cgroup", "r");

}

CgroupState
LoadSystemdCgroupState(unsigned pid) noexcept
{
	FILE *file = OpenProcCgroup(pid);
	if (file == nullptr)
		return CgroupState();

	AtScopeExit(file) { fclose(file); };

	struct ControllerAssignment {
		std::string name;
		std::string path;

		std::forward_list<std::string> controllers;

		ControllerAssignment(StringView _name, StringView _path)
			:name(_name.data, _name.size),
			 path(_path.data, _path.size) {}
	};

	std::forward_list<ControllerAssignment> assignments;

	std::string systemd_path;
	bool have_unified = false;

	char line[256];
	while (fgets(line, sizeof(line), file) != nullptr) {
		if (StringStartsWith(line, "0::/")) {
			have_unified = true;
			continue;
		}

		char *p = line, *endptr;

		strtoul(p, &endptr, 10);
		if (endptr == p || *endptr != ':')
			continue;

		char *const _name = endptr + 1;
		char *const colon = strchr(_name, ':');
		if (colon == nullptr || colon == _name ||
		    colon[1] != '/' || colon[2] == '/')
			continue;

		StringView name(_name, colon);

		StringView path(colon + 1);
		if (path.back() == '\n')
			--path.size;

		if (name.Equals("name=systemd"))
			systemd_path = std::string(path.data, path.size);
		else {
			assignments.emplace_front(name, path);

			auto &controllers = assignments.front().controllers;
			for (StringView i : IterableSplitString(name, ','))
				controllers.emplace_front(i.data, i.size);
		}
	}

	if (systemd_path.empty())
		/* no "systemd" controller found - disable the feature */
		return CgroupState();

	CgroupState state;

	for (auto &i : assignments) {
		if (i.path == systemd_path) {
			for (auto &controller : i.controllers)
				state.controllers.emplace(std::move(controller), i.name);

			state.mounts.emplace_front(std::move(i.name));
		}
	}

	state.mounts.emplace_front("systemd");

	if (have_unified)
		state.mounts.emplace_front("unified");

	// TODO: support pure unified hierarchy (no hybrid)

	state.group_path = std::move(systemd_path);

	return state;
}

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
				       Variant(Uint64(properties.memory_max))))
		.AppendOptional(properties.memory_low > 0,
				Struct(String("MemoryLow"),
				       Variant(Uint64(properties.memory_max))))
		.AppendOptional(properties.memory_high > 0,
				Struct(String("MemoryHigh"),
				       Variant(Uint64(properties.memory_max))))
		.AppendOptional(properties.memory_max > 0,
				Struct(String("MemoryMax"),
				       Variant(Uint64(properties.memory_max))))
		.AppendOptional(properties.memory_swap_max > 0,
				Struct(String("MemorySwapMax"),
				       Variant(Uint64(properties.memory_max))))
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
				Systemd::StopService(connection, name);
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
		? LoadSystemdCgroupState(0)
		: CgroupState();
}
