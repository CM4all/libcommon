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

#include "Client.hxx"
#include "IProtocol.hxx"
#include "Builder.hxx"
#include "Parser.hxx"
#include "Prepared.hxx"
#include "MountList.hxx"
#include "ExitListener.hxx"
#include "system/Error.hxx"
#include "util/RuntimeError.hxx"
#include "util/ScopeExit.hxx"

#include <array>

#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

static constexpr size_t MAX_FDS = 8;

SpawnServerClient::SpawnServerClient(EventLoop &event_loop,
                                     const SpawnConfig &_config,
                                     UniqueSocketDescriptor _socket,
                                     bool _verify)
    :config(_config), socket(std::move(_socket)),
     read_event(event_loop, socket.Get(),
                SocketEvent::READ|SocketEvent::PERSIST,
                BIND_THIS_METHOD(OnSocketEvent)),
     verify(_verify)
{
    read_event.Add();
}

SpawnServerClient::~SpawnServerClient()
{
    if (socket.IsDefined())
        Close();
}

void
SpawnServerClient::ReplaceSocket(UniqueSocketDescriptor new_socket)
{
    assert(socket.IsDefined());
    assert(new_socket.IsDefined());
    assert(!shutting_down);

    processes.clear();

    Close();

    socket = std::move(new_socket);

    read_event.Set(socket.Get(), SocketEvent::READ|SocketEvent::PERSIST);
    read_event.Add();
}

void
SpawnServerClient::Close()
{
    assert(socket.IsDefined());

    read_event.Delete();
    socket.Close();
}

void
SpawnServerClient::Shutdown()
{
    shutting_down = true;

    if (processes.empty() && socket.IsDefined())
        Close();
}

void
SpawnServerClient::CheckOrAbort()
{
    if (!socket.IsDefined()) {
        fprintf(stderr, "SpawnChildProcess: the spawner is gone, emergency!\n");
        exit(EXIT_FAILURE);
    }
}

inline void
SpawnServerClient::Send(ConstBuffer<void> payload, ConstBuffer<int> fds)
{
    ::Send<MAX_FDS>(socket.Get(), payload, fds);
}

inline void
SpawnServerClient::Send(const SpawnSerializer &s)
{
    ::Send<MAX_FDS>(socket.Get(), s);
}

UniqueSocketDescriptor
SpawnServerClient::Connect()
{
    CheckOrAbort();

    UniqueSocketDescriptor local_socket, remote_socket;
    if (!UniqueSocketDescriptor::CreateSocketPairNonBlock(AF_LOCAL, SOCK_SEQPACKET, 0,
                                                          local_socket,
                                                          remote_socket))
        throw MakeErrno("socketpair() failed");

    static constexpr SpawnRequestCommand cmd = SpawnRequestCommand::CONNECT;

    const int remote_fd = remote_socket.Get();

    try {
        Send(ConstBuffer<void>(&cmd, sizeof(cmd)), {&remote_fd, 1});
    } catch (...) {
        std::throw_with_nested(std::runtime_error("Spawn server failed"));
    }

    return local_socket;
}

static void
Serialize(SpawnSerializer &s, const CgroupOptions &c)
{
    s.WriteOptionalString(SpawnExecCommand::CGROUP, c.name);

    for (const auto *set = c.set_head; set != nullptr; set = set->next) {
        s.Write(SpawnExecCommand::CGROUP_SET);
        s.WriteString(set->name);
        s.WriteString(set->value);
    }
}

static void
Serialize(SpawnSerializer &s, const RefenceOptions &_r)
{
    const auto r = _r.Get();
    if (r != nullptr) {
        s.Write(SpawnExecCommand::REFENCE);
        s.Write(r.ToVoid());
        s.WriteByte(0);
    }
}

static void
Serialize(SpawnSerializer &s, const NamespaceOptions &ns)
{
    s.WriteOptional(SpawnExecCommand::USER_NS, ns.enable_user);
    s.WriteOptional(SpawnExecCommand::PID_NS, ns.enable_pid);
    s.WriteOptional(SpawnExecCommand::NETWORK_NS, ns.enable_network);
    s.WriteOptionalString(SpawnExecCommand::NETWORK_NS_NAME,
                          ns.network_namespace);
    s.WriteOptional(SpawnExecCommand::IPC_NS, ns.enable_ipc);
    s.WriteOptional(SpawnExecCommand::MOUNT_NS, ns.enable_mount);
    s.WriteOptional(SpawnExecCommand::MOUNT_PROC, ns.mount_proc);
    s.WriteOptional(SpawnExecCommand::WRITABLE_PROC, ns.writable_proc);
    s.WriteOptionalString(SpawnExecCommand::PIVOT_ROOT, ns.pivot_root);

    if (ns.mount_home != nullptr) {
        s.Write(SpawnExecCommand::MOUNT_HOME);
        s.WriteString(ns.mount_home);
        s.WriteString(ns.home);
    }

    s.WriteOptionalString(SpawnExecCommand::MOUNT_TMP_TMPFS, ns.mount_tmp_tmpfs);
    s.WriteOptionalString(SpawnExecCommand::MOUNT_TMPFS, ns.mount_tmpfs);

    for (auto i = ns.mounts; i != nullptr; i = i->next) {
        s.Write(SpawnExecCommand::BIND_MOUNT);
        s.WriteString(i->source);
        s.WriteString(i->target);
        s.WriteByte(i->writable);
        s.WriteByte(i->exec);
    }

    s.WriteOptionalString(SpawnExecCommand::HOSTNAME, ns.hostname);
}

static void
Serialize(SpawnSerializer &s, unsigned i, const ResourceLimit &rlimit)
{
    if (rlimit.IsEmpty())
        return;

    s.Write(SpawnExecCommand::RLIMIT);
    s.WriteByte(i);

    const struct rlimit &data = rlimit;
    s.WriteT(data);
}

static void
Serialize(SpawnSerializer &s, const ResourceLimits &rlimits)
{
    for (unsigned i = 0; i < RLIM_NLIMITS; ++i)
        Serialize(s, i, rlimits.values[i]);
}

static void
Serialize(SpawnSerializer &s, const UidGid &uid_gid)
{
    if (uid_gid.IsEmpty())
        return;

    s.Write(SpawnExecCommand::UID_GID);
    s.WriteT(uid_gid.uid);
    s.WriteT(uid_gid.gid);

    const size_t n_groups = uid_gid.CountGroups();
    s.WriteByte(n_groups);
    for (size_t i = 0; i < n_groups; ++i)
        s.WriteT(uid_gid.groups[i]);
}

static void
Serialize(SpawnSerializer &s, const PreparedChildProcess &p)
{
    assert(p.exec_function == nullptr); // not supported

    s.WriteOptionalString(SpawnExecCommand::HOOK_INFO, p.hook_info);

    for (const char *i : p.args)
        s.WriteString(SpawnExecCommand::ARG, i);

    for (const char *i : p.env)
        s.WriteString(SpawnExecCommand::SETENV, i);

    if (p.umask >= 0) {
        uint16_t umask = p.umask;
        s.Write(SpawnExecCommand::UMASK);
        s.WriteT(umask);
    }

    s.CheckWriteFd(SpawnExecCommand::STDIN, p.stdin_fd);
    s.CheckWriteFd(SpawnExecCommand::STDOUT, p.stdout_fd);
    s.CheckWriteFd(SpawnExecCommand::STDERR, p.stderr_fd);
    s.CheckWriteFd(SpawnExecCommand::CONTROL, p.control_fd);

    s.WriteOptionalString(SpawnExecCommand::STDERR_PATH, p.stderr_path);

    if (p.priority != 0) {
        s.Write(SpawnExecCommand::PRIORITY);
        s.WriteInt(p.priority);
    }

    Serialize(s, p.cgroup);
    Serialize(s, p.refence);
    Serialize(s, p.ns);
    Serialize(s, p.rlimits);
    Serialize(s, p.uid_gid);

    s.WriteOptionalString(SpawnExecCommand::CHROOT, p.chroot);
    s.WriteOptionalString(SpawnExecCommand::CHDIR, p.chdir);

    if (p.sched_idle)
        s.Write(SpawnExecCommand::SCHED_IDLE_);

    if (p.ioprio_idle)
        s.Write(SpawnExecCommand::IOPRIO_IDLE);

    if (p.forbid_user_ns)
        s.Write(SpawnExecCommand::FORBID_USER_NS);

    if (p.forbid_multicast)
        s.Write(SpawnExecCommand::FORBID_MULTICAST);

    if (p.forbid_bind)
        s.Write(SpawnExecCommand::FORBID_BIND);

    if (p.no_new_privs)
        s.Write(SpawnExecCommand::NO_NEW_PRIVS);

    if (p.tty)
        s.Write(SpawnExecCommand::TTY);
}

int
SpawnServerClient::SpawnChildProcess(const char *name,
                                     PreparedChildProcess &&p,
                                     ExitListener *listener)
{
    assert(!shutting_down);

    /* this check is performed again on the server (which is obviously
       necessary, and the only way to have it secure); this one is
       only here for the developer to see the error earlier in the
       call chain */
    if (verify && !p.uid_gid.IsEmpty())
        config.Verify(p.uid_gid);

    CheckOrAbort();

    const int pid = MakePid();

    SpawnSerializer s(SpawnRequestCommand::EXEC);

    try {
        s.WriteInt(pid);
        s.WriteString(name);

        Serialize(s, p);
    } catch (SpawnPayloadTooLargeError) {
        throw std::runtime_error("Spawn payload is too large");
    }

    try {
        Send(s.GetPayload(), s.GetFds());
    } catch (const std::runtime_error &e) {
        std::throw_with_nested(std::runtime_error("Spawn server failed"));
    }

    processes.emplace(std::piecewise_construct,
                      std::forward_as_tuple(pid),
                      std::forward_as_tuple(listener));
    return pid;
}

void
SpawnServerClient::SetExitListener(int pid, ExitListener *listener)
{
    auto i = processes.find(pid);
    assert(i != processes.end());

    assert(i->second.listener == nullptr);
    i->second.listener = listener;
}

void
SpawnServerClient::KillChildProcess(int pid, int signo)
{
    CheckOrAbort();

    auto i = processes.find(pid);
    assert(i != processes.end());
    assert(i->second.listener != nullptr);
    processes.erase(i);

    SpawnSerializer s(SpawnRequestCommand::KILL);
    s.WriteInt(pid);
    s.WriteInt(signo);

    try {
        Send(s.GetPayload(), s.GetFds());
    } catch (const std::runtime_error &e) {
        fprintf(stderr, "failed to send KILL(%d) to spawner: %s\n",
                pid,e.what());
    }

    if (shutting_down && processes.empty())
        Close();
}

inline void
SpawnServerClient::HandleExitMessage(SpawnPayload payload)
{
    int pid, status;
    payload.ReadInt(pid);
    payload.ReadInt(status);
    if (!payload.IsEmpty())
        throw MalformedSpawnPayloadError();

    auto i = processes.find(pid);
    if (i == processes.end())
        return;

    auto *listener = i->second.listener;
    processes.erase(i);

    if (listener != nullptr)
        listener->OnChildProcessExit(status);

    if (shutting_down && processes.empty())
        Close();
}

inline void
SpawnServerClient::HandleMessage(ConstBuffer<uint8_t> payload)
{
    const auto cmd = (SpawnResponseCommand)payload.shift();

    switch (cmd) {
    case SpawnResponseCommand::CGROUPS_AVAILABLE:
        cgroups = true;
        break;

    case SpawnResponseCommand::EXIT:
        HandleExitMessage(SpawnPayload(payload));
        break;
    }
}

inline void
SpawnServerClient::OnSocketEvent(unsigned)
{
    constexpr size_t N = 64;
    std::array<uint8_t[16], N> payloads;
    std::array<struct iovec, N> iovs;
    std::array<struct mmsghdr, N> msgs;

    for (size_t i = 0; i < N; ++i) {
        auto &iov = iovs[i];
        iov.iov_base = payloads[i];
        iov.iov_len = sizeof(payloads[i]);

        auto &msg = msgs[i].msg_hdr;
        msg.msg_name = nullptr;
        msg.msg_namelen = 0;
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_control = nullptr;
        msg.msg_controllen = 0;
    }

    int n = recvmmsg(socket.Get(), &msgs.front(), msgs.size(),
                     MSG_DONTWAIT|MSG_CMSG_CLOEXEC, nullptr);
    if (n <= 0) {
        if (n < 0)
            fprintf(stderr, "recvmsg() from spawner failed: %s\n",
                    strerror(errno));
        else
            fprintf(stderr, "spawner closed the socket\n");
        Close();
        return;
    }

    for (int i = 0; i < n; ++i) {
        if (msgs[i].msg_len == 0) {
            /* when the peer closes the socket, recvmmsg() doesn't
               return 0; insteaed, it fills the mmsghdr array with
               empty packets */
            fprintf(stderr, "spawner closed the socket\n");
            Close();
            return;
        }

        HandleMessage({payloads[i], msgs[i].msg_len});
    }
}
