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

#include "CgroupOptions.hxx"
#include "CgroupState.hxx"
#include "Config.hxx"
#include "AllocatorPtr.hxx"
#include "system/Error.hxx"
#include "io/WriteFile.hxx"
#include "util/StringView.hxx"
#include "util/RuntimeError.hxx"

#include <assert.h>
#include <sched.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <limits.h>

#ifndef __linux
#error This library requires Linux
#endif

CgroupOptions::CgroupOptions(AllocatorPtr alloc, const CgroupOptions &src)
    :name(alloc.CheckDup(src.name))
{
    auto **set_tail = &set_head;

    for (const auto *i = src.set_head; i != nullptr; i = i->next) {
        auto *new_set = alloc.New<SetItem>(alloc.Dup(i->name),
                                           alloc.Dup(i->value));
        *set_tail = new_set;
        set_tail = &new_set->next;
    }
}

void
CgroupOptions::Set(AllocatorPtr alloc, StringView _name, StringView _value)
{
    auto *new_set = alloc.New<SetItem>(alloc.DupZ(_name), alloc.DupZ(_value));
    new_set->next = set_head;
    set_head = new_set;
}

static void
WriteFile(const char *path, const char *data)
{
    if (TryWriteExistingFile(path, data) == WriteFileResult::ERROR)
        throw FormatErrno("write('%s') failed", path);
}

static void
MoveToNewCgroup(const char *mount_base_path, const char *controller,
                const char *delegated_group, const char *sub_group)
{
    char path[PATH_MAX];

    constexpr int max_path = sizeof(path) - 16;
    if (snprintf(path, max_path, "%s/%s%s/%s",
                 mount_base_path, controller,
                 delegated_group, sub_group) >= max_path)
        throw std::runtime_error("Path is too long");

    if (mkdir(path, 0777) < 0) {
        switch (errno) {
        case EEXIST:
            break;

        default:
            throw FormatErrno("mkdir('%s') failed", path);
        }
    }

    strcat(path, "/cgroup.procs");
    WriteFile(path, "0");
}

void
CgroupOptions::Apply(const CgroupState &state) const
{
    if (name == nullptr)
        return;

    if (!state.IsEnabled())
        throw std::runtime_error("Control groups are disabled");

    const auto mount_base_path = "/sys/fs/cgroup";

    for (const auto &mount_point : state.mounts)
        MoveToNewCgroup(mount_base_path, mount_point.c_str(),
                        state.group_path.c_str(), name);

    // TODO: move to "name=systemd"?

    for (const auto *set = set_head; set != nullptr; set = set->next) {
        const char *dot = strchr(set->name, '.');
        assert(dot != nullptr);

        const std::string controller(set->name, dot);
        auto i = state.controllers.find(controller);
        if (i == state.controllers.end())
            throw FormatRuntimeError("cgroup controller '%s' is unavailable",
                                     controller.c_str());

        const std::string &mount_point = i->second;

        char path[PATH_MAX];

        if (snprintf(path, sizeof(path), "%s/%s%s/%s/%s",
                     mount_base_path, mount_point.c_str(),
                     state.group_path.c_str(), name,
                     set->name) >= (int)sizeof(path))
            throw std::runtime_error("Path is too long");

        WriteFile(path, set->value);
    }
}

char *
CgroupOptions::MakeId(char *p) const
{
    if (name != nullptr) {
        p = (char *)mempcpy(p, ";cg", 3);
        p = stpcpy(p, name);
    }

    return p;
}
