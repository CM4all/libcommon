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

#include "NamespaceOptions.hxx"
#include "UserNamespace.hxx"
#include "NetworkNamespace.hxx"
#include "UidGid.hxx"
#include "MountList.hxx"
#include "AllocatorPtr.hxx"
#include "system/pivot_root.h"
#include "system/BindMount.hxx"
#include "system/Error.hxx"
#include "util/ScopeExit.hxx"

#if TRANSLATION_ENABLE_EXPAND
#include "pexpand.hxx"
#endif

#include <set>

#include <assert.h>
#include <sched.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/prctl.h>

#ifndef __linux
#error This library requires Linux
#endif

NamespaceOptions::NamespaceOptions(AllocatorPtr alloc,
                                   const NamespaceOptions &src)
    :enable_user(src.enable_user),
     enable_pid(src.enable_pid),
     enable_cgroup(src.enable_cgroup),
     enable_network(src.enable_network),
     enable_ipc(src.enable_ipc),
     enable_mount(src.enable_mount),
     mount_root_tmpfs(src.mount_root_tmpfs),
     mount_proc(src.mount_proc),
     writable_proc(src.writable_proc),
     mount_pts(src.mount_pts),
     bind_mount_pts(src.bind_mount_pts),
     network_namespace(alloc.CheckDup(src.network_namespace)),
     pivot_root(alloc.CheckDup(src.pivot_root)),
     home(alloc.CheckDup(src.home)),
#if TRANSLATION_ENABLE_EXPAND
     expand_home(alloc.CheckDup(src.expand_home)),
#endif
     mount_home(alloc.CheckDup(src.mount_home)),
     mount_tmp_tmpfs(alloc.CheckDup(src.mount_tmp_tmpfs)),
     mount_tmpfs(alloc.CheckDup(src.mount_tmpfs)),
     mounts(MountList::CloneAll(alloc, src.mounts)),
     hostname(alloc.CheckDup(src.hostname))
{
}

#if TRANSLATION_ENABLE_EXPAND

bool
NamespaceOptions::IsExpandable() const
{
    return expand_home != nullptr || MountList::IsAnyExpandable(mounts);
}

void
NamespaceOptions::Expand(AllocatorPtr alloc, const MatchInfo &match_info)
{
    if (expand_home != nullptr)
        home = expand_string_unescaped(alloc, expand_home, match_info);

    MountList::ExpandAll(alloc, mounts, match_info);
}

#endif

int
NamespaceOptions::GetCloneFlags(int flags) const
{
#ifndef CLONE_NEWCGROUP
    constexpr int CLONE_NEWCGROUP = 0x02000000;
#endif

    if (enable_user)
        flags |= CLONE_NEWUSER;
    if (enable_pid)
        flags |= CLONE_NEWPID;
    if (enable_cgroup)
        flags |= CLONE_NEWCGROUP;
    if (enable_network)
        flags |= CLONE_NEWNET;
    if (enable_ipc)
        flags |= CLONE_NEWIPC;
    if (enable_mount)
        flags |= CLONE_NEWNS;
    if (hostname != nullptr)
        flags |= CLONE_NEWUTS;

    return flags;
}

void
NamespaceOptions::SetupUidGidMap(const UidGid &uid_gid,
                                 int pid) const
{
    /* collect all gids (including supplementary groups) in a std::set
       to eliminate duplicates, and then map them all into the new
       user namespace */
    std::set<unsigned> gids;
    gids.emplace(uid_gid.gid);
    for (unsigned i = 0; uid_gid.groups[i] != 0; ++i)
        gids.emplace(uid_gid.groups[i]);

    /* always map the "root" group or else file operations may fail
       with EOVERFLOW (and this method will only be called if we're
       root) */
    gids.emplace(0);

    SetupGidMap(pid, gids);

    /* always map the "root" user or else file operations may fail
       with EOVERFLOW (and this method will only be called if we're
       root) */
    SetupUidMap(pid, uid_gid.uid, true);
}

void
NamespaceOptions::ReassociateNetwork() const
{
    assert(network_namespace != nullptr);

    ReassociateNetworkNamespace(network_namespace);
}

static void
ChdirOrThrow(const char *path)
{
    if (chdir(path) < 0)
        throw FormatErrno("chdir('%s') failed", path);
}

static void
MakeDirs(const char *path)
{
    assert(path != nullptr);
    assert(*path == '/');

    ++path; /* skip the slash to make it relative */

    char *allocated = strdup(path);
    AtScopeExit(allocated) { free(allocated); };

    char *p = allocated;
    while (char *slash = strchr(p, '/')) {
        *slash = 0;
        mkdir(allocated, 0755);
        *slash = '/';
        p = slash + 1;
    }

    mkdir(allocated, 0700);
}

static void
MountOrThrow(const char *source, const char *target,
             const char *filesystemtype, unsigned long mountflags,
             const void *data)
{
    if (mount(source, target, filesystemtype, mountflags, data) < 0)
        throw FormatErrno("mount('%s') failed", target);
}

void
NamespaceOptions::Setup(const UidGid &uid_gid) const
{
    /* set up UID/GID mapping in the old /proc */
    if (enable_user) {
        DenySetGroups();

        if (uid_gid.gid != 0)
            SetupGidMap(0, uid_gid.gid, false);
        // TODO: map the current effective gid if no gid was given?

        SetupUidMap(0, uid_gid.uid, false);
    }

    if (network_namespace != nullptr)
        ReassociateNetwork();

    if (enable_mount)
        /* convert all "shared" mounts to "private" mounts */
        mount(nullptr, "/", nullptr, MS_PRIVATE|MS_REC, nullptr);

    const char *const put_old = "/mnt";

    const char *new_root = nullptr;

    if (pivot_root != nullptr) {
        /* first bind-mount the new root onto itself to "unlock" the
           kernel's mount object (flag MNT_LOCKED) in our namespace;
           without this, the kernel would not allow an unprivileged
           process to pivot_root to it */

        new_root = pivot_root;
        BindMount(new_root, new_root, MS_NOSUID|MS_RDONLY);

        /* release a reference to the old root */
        ChdirOrThrow(new_root);
    } else if (mount_root_tmpfs) {
        new_root = "/tmp";

        /* create an empty tmpfs as the new filesystem root */
        MountOrThrow("none", new_root, "tmpfs", MS_NODEV|MS_NOEXEC|MS_NOSUID,
                     "size=256k,nr_inodes=1024,mode=755");

        ChdirOrThrow(new_root);

        /* create all mountpoints which we'll need later on */

        const auto old_umask = umask(0022);
        AtScopeExit(old_umask) { umask(old_umask); };

        mkdir(put_old + 1, 0700);

        if (mount_proc)
            mkdir("proc", 0700);

        if (mount_pts || bind_mount_pts) {
            mkdir("dev", 0755);
            mkdir("dev/pts", 0700);
        }

        if (mount_home != nullptr)
            MakeDirs(mount_home);

        if (mount_tmp_tmpfs != nullptr)
            mkdir("tmp", 0700);

        for (const auto *i = mounts; i != nullptr; i = i->next)
            MakeDirs(i->target);

        if (mount_tmpfs != nullptr)
            MakeDirs(mount_tmpfs);
    }

    if (new_root != nullptr) {
        /* enter the new root */
        int result = my_pivot_root(new_root, put_old + 1);
        if (result < 0)
            throw FormatErrno("pivot_root('%s') failed", new_root);
    }

    if (mount_proc) {
        unsigned long flags = MS_NOEXEC|MS_NOSUID|MS_NODEV;
        if (!writable_proc)
            flags |= MS_RDONLY;

        MountOrThrow("proc", "/proc", "proc", flags, nullptr);
    }

    if (mount_pts)
        /* the "newinstance" option is only needed with pre-4.7
           kernels; from v4.7 on, this is implicit for all new devpts
           mounts (kernel commit eedf265aa003) */
        MountOrThrow("devpts", "/dev/pts", "devpts",
                     MS_NOEXEC|MS_NOSUID,
                     "newinstance");

    if (HasBindMount()) {
        /* go to /mnt so we can refer to the old directories with a
           relative path */
        ChdirOrThrow(new_root != nullptr ? put_old : "/");

        if (bind_mount_pts)
            BindMount("dev/pts", "/dev/pts", MS_NOSUID|MS_NOEXEC);

        if (mount_home != nullptr) {
            assert(home != nullptr);
            assert(*home == '/');

            BindMount(home + 1, mount_home, MS_NOSUID|MS_NODEV);
        }

        MountList::ApplyAll(mounts);

        if (new_root != nullptr)
            /* back to the new root */
            ChdirOrThrow("/");
    }

    if (new_root != nullptr &&
        /* get rid of the old root */
        umount2(put_old, MNT_DETACH) < 0)
        throw FormatErrno("umount('%s') failed: %s", put_old);

    if (mount_root_tmpfs) {
        rmdir(put_old);

        /* make the root tmpfs read-only */

        if (mount(nullptr, "/", nullptr,
                  MS_REMOUNT|MS_BIND|MS_NODEV|MS_NOEXEC|MS_NOSUID|MS_RDONLY,
                  nullptr) < 0)
            throw MakeErrno("Failed to remount read-only");
    }

    if (mount_tmpfs != nullptr)
        MountOrThrow("none", mount_tmpfs, "tmpfs",
                     MS_NODEV|MS_NOEXEC|MS_NOSUID,
                     "size=16M,nr_inodes=256,mode=700");

    if (mount_tmp_tmpfs != nullptr) {
        const char *options = "size=16M,nr_inodes=256,mode=1777";
        char buffer[256];
        if (*mount_tmp_tmpfs != 0) {
            snprintf(buffer, sizeof(buffer), "%s,%s", options, mount_tmp_tmpfs);
            options = buffer;
        }

        MountOrThrow("none", "/tmp", "tmpfs",
                     MS_NODEV|MS_NOEXEC|MS_NOSUID,
                     options);
    }

    if (hostname != nullptr &&
        sethostname(hostname, strlen(hostname)) < 0)
        throw MakeErrno("sethostname() failed");
}

char *
NamespaceOptions::MakeId(char *p) const
{
    if (enable_user)
        p = (char *)mempcpy(p, ";uns", 4);

    if (enable_pid)
        p = (char *)mempcpy(p, ";pns", 4);

    if (enable_cgroup)
        p = (char *)mempcpy(p, ";cns", 4);

    if (enable_network) {
        p = (char *)mempcpy(p, ";nns", 4);

        if (network_namespace != nullptr) {
            *p++ = '=';
            p = stpcpy(p, network_namespace);
        }
    }

    if (enable_ipc)
        p = (char *)mempcpy(p, ";ins", 4);

    if (enable_mount) {
        p = (char *)(char *)mempcpy(p, ";mns", 4);

        if (pivot_root != nullptr) {
            p = (char *)mempcpy(p, ";pvr=", 5);
            p = stpcpy(p, pivot_root);
        }

        if (mount_root_tmpfs)
            p = (char *)mempcpy(p, ";rt", 3);

        if (mount_proc) {
            p = (char *)mempcpy(p, ";proc", 5);
            if (writable_proc)
                *p++ = 'w';
        }

        if (mount_pts)
            p = (char *)mempcpy(p, ";pts", 4);

        if (bind_mount_pts)
            p = (char *)mempcpy(p, ";bpts", 4);

        if (mount_home != nullptr) {
            p = (char *)mempcpy(p, ";h:", 3);
            p = stpcpy(p, home);
            *p++ = '=';
            p = stpcpy(p, mount_home);
        }

        if (mount_tmp_tmpfs != nullptr) {
            p = (char *)mempcpy(p, ";tt:", 3);
            p = stpcpy(p, mount_tmp_tmpfs);
        }

        if (mount_tmpfs != nullptr) {
            p = (char *)mempcpy(p, ";t:", 3);
            p = stpcpy(p, mount_tmpfs);
        }

        p = MountList::MakeIdAll(p, mounts);
    }

    if (hostname != nullptr) {
        p = (char *)mempcpy(p, ";uts=", 5);
        p = stpcpy(p, hostname);
    }

    return p;
}
