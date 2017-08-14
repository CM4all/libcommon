/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "NamespaceOptions.hxx"
#include "UidGid.hxx"
#include "MountList.hxx"
#include "AllocatorPtr.hxx"
#include "system/pivot_root.h"
#include "system/BindMount.hxx"
#include "system/Error.hxx"
#include "io/WriteFile.hxx"
#include "io/UniqueFileDescriptor.hxx"
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
     mount_proc(src.mount_proc),
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

static void
write_file(const char *path, const char *data)
{
    if (TryWriteExistingFile(path, data) == WriteFileResult::ERROR)
        throw FormatErrno("write('%s') failed", path);
}

static void
setup_uid_map(int uid)
{
    char buffer[64];
    sprintf(buffer, "%d %d 1", uid, uid);
    write_file("/proc/self/uid_map", buffer);
}

static void
setup_gid_map(int gid)
{
    char buffer[64];
    sprintf(buffer, "%d %d 1", gid, gid);
    write_file("/proc/self/gid_map", buffer);
}

/**
 * Write "deny" to /proc/self/setgroups which is necessary for
 * unprivileged processes to set up a gid_map.  See Linux commits
 * 9cc4651 and 66d2f33 for details.
 */
static void
deny_setgroups()
{
    TryWriteExistingFile("/proc/self/setgroups", "deny");
}

void
NamespaceOptions::SetupUidGidMap(const UidGid &uid_gid,
                                 int pid) const
{
    char path[64], buffer[1024];

    /* collect all gids (including supplementary groups) in a std::set
       to eliminate duplicates, and then map them all into the new
       user namespace */
    std::set<int> gids;
    gids.emplace(uid_gid.gid);
    for (unsigned i = 0; uid_gid.groups[i] != 0; ++i)
        gids.emplace(uid_gid.groups[i]);

    size_t position = 0;
    for (int i : gids) {
        if (position + 64 > sizeof(buffer))
            break;

        position += sprintf(buffer + position, "%d %d 1\n", i, i);
    }

    sprintf(path, "/proc/%d/gid_map", pid);
    write_file(path, buffer);

    const int uid = uid_gid.uid;
    sprintf(path, "/proc/%d/uid_map", pid);
    sprintf(buffer, "%d %d 1", uid, uid);
    write_file(path, buffer);
}

/**
 * Open a network namespace in /run/netns.
 */
static UniqueFileDescriptor
OpenNetworkNS(const char *name)
{
    char path[4096];
    if (snprintf(path, sizeof(path),
                 "/run/netns/%s", name) >= (int)sizeof(path))
        throw std::runtime_error("Network namespace name is too long");

    UniqueFileDescriptor fd;
    if (!fd.OpenReadOnly(path))
        throw FormatErrno("Failed to open %s", path);

    return fd;
}

void
NamespaceOptions::ReassociateNetwork() const
{
    assert(network_namespace != nullptr);

    if (setns(OpenNetworkNS(network_namespace).Get(), CLONE_NEWNET) < 0)
        throw MakeErrno("Failed to reassociate with network namespace");
}

void
NamespaceOptions::Setup(const UidGid &uid_gid) const
{
    /* set up UID/GID mapping in the old /proc */
    if (enable_user) {
        deny_setgroups();

        if (uid_gid.gid != 0)
            setup_gid_map(uid_gid.gid);
        // TODO: map the current effective gid if no gid was given?

        setup_uid_map(uid_gid.uid);
    }

    if (network_namespace != nullptr)
        ReassociateNetwork();

    if (enable_mount)
        /* convert all "shared" mounts to "private" mounts */
        mount(nullptr, "/", nullptr, MS_PRIVATE|MS_REC, nullptr);

    const char *const new_root = pivot_root;
    const char *const put_old = "mnt";

    if (new_root != nullptr) {
        /* first bind-mount the new root onto itself to "unlock" the
           kernel's mount object (flag MNT_LOCKED) in our namespace;
           without this, the kernel would not allow an unprivileged
           process to pivot_root to it */
        BindMount(new_root, new_root, MS_NOSUID|MS_RDONLY);

        /* release a reference to the old root */
        if (chdir(new_root) < 0)
            throw FormatErrno("chdir('%s') failed", new_root);

        /* enter the new root */
        int result = my_pivot_root(new_root, put_old);
        if (result < 0)
            throw FormatErrno(-result, "pivot_root('%s') failed", new_root);
    }

    if (mount_proc &&
        mount("proc", "/proc", "proc", MS_NOEXEC|MS_NOSUID|MS_NODEV|MS_RDONLY,
              nullptr) < 0)
        throw MakeErrno("mount('/proc') failed");

    if (bind_mount_pts)
        BindMount("/mnt/dev/pts", "/dev/pts", MS_NOSUID|MS_NOEXEC);

    if (mount_pts &&
        mount("devpts", "/dev/pts", "devpts", MS_NOEXEC|MS_NOSUID,
              nullptr) < 0)
        throw MakeErrno("mount('/dev/pts') failed");

    if (HasBindMount()) {
        /* go to /mnt so we can refer to the old directories with a
           relative path */

        const char *path = new_root != nullptr ? "/mnt" : "/";

        if (chdir(path) < 0)
            throw FormatErrno("chdir('%s') failed", path);
    }

    if (mount_home != nullptr) {
        assert(home != nullptr);
        assert(*home == '/');

        BindMount(home + 1, mount_home, MS_NOSUID|MS_NODEV);
    }

    MountList::ApplyAll(mounts);

    if (new_root != nullptr && HasBindMount() &&
        /* back to the new root */
        chdir("/") < 0)
        throw MakeErrno("chdir('/') failed");

    if (new_root != nullptr &&
        /* get rid of the old root */
        umount2(put_old, MNT_DETACH) < 0)
        throw FormatErrno("umount('%s') failed: %s", put_old);

    if (mount_tmpfs != nullptr &&
        mount("none", mount_tmpfs, "tmpfs", MS_NODEV|MS_NOEXEC|MS_NOSUID,
              "size=16M,nr_inodes=256,mode=700") < 0)
        throw FormatErrno("mount(tmpfs, '%s') failed", mount_tmpfs);

    if (mount_tmp_tmpfs != nullptr) {
        const char *options = "size=16M,nr_inodes=256,mode=1777";
        char buffer[256];
        if (*mount_tmp_tmpfs != 0) {
            snprintf(buffer, sizeof(buffer), "%s,%s", options, mount_tmp_tmpfs);
            options = buffer;
        }

        if (mount("none", "/tmp", "tmpfs", MS_NODEV|MS_NOEXEC|MS_NOSUID,
                  options) < 0)
            throw MakeErrno("mount('/tmp') failed");
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

        if (mount_proc)
            p = (char *)mempcpy(p, ";proc", 5);

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
