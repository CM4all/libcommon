/*
 * Copyright 2007-2017 Content Management AG
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
#include "odbus/Connection.hxx"
#include "odbus/Message.hxx"
#include "odbus/AppendIter.hxx"
#include "odbus/ReadIter.hxx"
#include "odbus/PendingCall.hxx"
#include "odbus/Error.hxx"
#include "util/Macros.hxx"
#include "util/IterableSplitString.hxx"
#include "util/ScopeExit.hxx"

#include <systemd/sd-daemon.h>

#include <forward_list>

#include <unistd.h>
#include <stdio.h>

gcc_pure
static CgroupState
LoadSystemdDelegate()
{
    FILE *file = fopen("/proc/self/cgroup", "r");
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

    char line[256];
    while (fgets(line, sizeof(line), file) != nullptr) {
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

    state.group_path = std::move(systemd_path);

    return state;
}

static void
WaitJobRemoved(DBusConnection *connection, const char *object_path)
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
            DBusError err;
            dbus_error_init(&err);

            dbus_uint32_t job_id;
            const char *removed_object_path, *unit_name, *result_string;
            if (!msg.GetArgs(err,
                             DBUS_TYPE_UINT32, &job_id,
                             DBUS_TYPE_OBJECT_PATH, &removed_object_path,
                             DBUS_TYPE_STRING, &unit_name,
                             DBUS_TYPE_STRING, &result_string)) {
                fprintf(stderr, "JobRemoved failed: %s\n", err.message);
                dbus_error_free(&err);
                break;
            }

            if (strcmp(removed_object_path, object_path) == 0)
                break;
        }
    }
}

CgroupState
CreateSystemdScope(const char *name, const char *description,
                   int pid, bool delegate)
{
    if (!sd_booted())
        return CgroupState();

    ODBus::Error error;

    auto connection = ODBus::Connection::GetSystem();

    const char *match = "type='signal',"
        "sender='org.freedesktop.systemd1',"
        "interface='org.freedesktop.systemd1.Manager',"
        "member='JobRemoved',"
        "path='/org/freedesktop/systemd1'";
    dbus_bus_add_match(connection, match, error);
    error.CheckThrow("DBus AddMatch error");

    AtScopeExit(&connection, match){
        dbus_bus_remove_match(connection, match, nullptr);
    };

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

    AppendMessageIter(args, DBUS_TYPE_ARRAY, PropTypeTraits::TypeAsString::value)
        .Append(Struct(String("Description"),
                       Variant(String(description))))
        .Append(Struct(String("PIDs"),
                       Variant(FixedArray(pids_value, ARRAY_SIZE(pids_value)))))
        .Append(Struct(String("Delegate"),
                       Variant(Boolean(delegate))))
        .CloseContainer(args);

    using AuxTypeTraits = StructTypeTraits<StringTypeTraits,
                                           ArrayTypeTraits<StructTypeTraits<StringTypeTraits,
                                                                            VariantTypeTraits>>>;
    args.AppendEmptyArray<AuxTypeTraits>();

    auto pending = PendingCall::SendWithReply(connection, msg.Get());

    dbus_connection_flush(connection);

    pending.Block();

    Message reply = Message::StealReply(*pending.Get());
    reply.CheckThrowError();

    const char *object_path;
    if (!reply.GetArgs(error, DBUS_TYPE_OBJECT_PATH, &object_path))
        error.Throw("StartTransientUnit reply failed");

    WaitJobRemoved(connection, object_path);

    return delegate
        ? LoadSystemdDelegate()
        : CgroupState();
}
