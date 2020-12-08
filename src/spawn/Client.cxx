/*
 * Copyright 2017-2020 CM4all GmbH
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
#include "Handler.hxx"
#include "IProtocol.hxx"
#include "Builder.hxx"
#include "Parser.hxx"
#include "Prepared.hxx"
#include "CgroupOptions.hxx"
#include "Mount.hxx"
#include "ExitListener.hxx"
#include "system/Error.hxx"
#include "util/PrintException.hxx"

#include <array>
#include <stdexcept>

#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>

static constexpr size_t MAX_FDS = 8;

SpawnServerClient::SpawnServerClient(EventLoop &event_loop,
				     const SpawnConfig &_config,
				     UniqueSocketDescriptor _socket,
				     bool _verify) noexcept
	:config(_config),
	 event(event_loop, BIND_THIS_METHOD(OnSocketEvent), _socket.Release()),
	 verify(_verify)
{
	event.ScheduleRead();
}

SpawnServerClient::~SpawnServerClient() noexcept
{
	event.Close();
}

void
SpawnServerClient::ReplaceSocket(UniqueSocketDescriptor new_socket) noexcept
{
	assert(event.IsDefined());
	assert(new_socket.IsDefined());
	assert(!shutting_down);

	processes.clear();

	Close();

	event.Open(new_socket.Release());
	event.ScheduleRead();
}

void
SpawnServerClient::Close() noexcept
{
	assert(event.IsDefined());

	event.Close();
}

void
SpawnServerClient::Shutdown() noexcept
{
	shutting_down = true;

	if (processes.empty() && event.IsDefined())
		Close();
}

void
SpawnServerClient::CheckOrAbort() noexcept
{
	if (!event.IsDefined()) {
		fprintf(stderr, "SpawnChildProcess: the spawner is gone, emergency!\n");
		_exit(EXIT_FAILURE);
	}
}

inline void
SpawnServerClient::Send(ConstBuffer<void> payload, ConstBuffer<int> fds)
{
	::Send<MAX_FDS>(event.GetSocket(), payload, fds);
}

inline void
SpawnServerClient::Send(const SpawnSerializer &s)
{
	::Send<MAX_FDS>(event.GetSocket(), s);
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
	s.WriteOptionalString(SpawnExecCommand::CGROUP_SESSION, c.session);

	for (const auto &i : c.set) {
		s.Write(SpawnExecCommand::CGROUP_SET);
		s.WriteString(i.name);
		s.WriteString(i.value);
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
	s.WriteOptional(SpawnExecCommand::MOUNT_PROC, ns.mount.mount_proc);
	s.WriteOptional(SpawnExecCommand::WRITABLE_PROC,
			ns.mount.writable_proc);
	s.WriteOptionalString(SpawnExecCommand::PIVOT_ROOT,
			      ns.mount.pivot_root);

	if (ns.mount.mount_home != nullptr) {
		s.Write(SpawnExecCommand::MOUNT_HOME);
		s.WriteString(ns.mount.mount_home);
		s.WriteString(ns.mount.home);
	}

	s.WriteOptionalString(SpawnExecCommand::MOUNT_TMP_TMPFS,
			      ns.mount.mount_tmp_tmpfs);

	for (const auto &i : ns.mount.mounts) {
		switch (i.type) {
		case Mount::Type::BIND:
			s.Write(SpawnExecCommand::BIND_MOUNT);
			s.WriteString(i.source);
			s.WriteString(i.target);
			s.WriteByte(i.writable);
			s.WriteByte(i.exec);
			break;

		case Mount::Type::TMPFS:
			s.WriteString(SpawnExecCommand::MOUNT_TMPFS,
				      i.target);
			s.WriteByte(i.writable);
			break;
		}
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

	if (p.cgroup != nullptr)
		Serialize(s, *p.cgroup);
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
				     SocketDescriptor return_stderr,
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

		if (return_stderr.IsDefined())
			s.CheckWriteFd(SpawnExecCommand::RETURN_STDERR,
				       return_stderr.Get());
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
SpawnServerClient::SetExitListener(int pid, ExitListener *listener) noexcept
{
	auto i = processes.find(pid);
	assert(i != processes.end());

	assert(i->second.listener == nullptr);
	i->second.listener = listener;
}

void
SpawnServerClient::KillChildProcess(int pid, int signo) noexcept
{
	CheckOrAbort();

	auto i = processes.find(pid);
	assert(i != processes.end());
	assert(i->second.listener != nullptr);
	processes.erase(i);

	try {
		SpawnSerializer s(SpawnRequestCommand::KILL);
		s.WriteInt(pid);
		s.WriteInt(signo);

		try {
			Send(s.GetPayload(), s.GetFds());
		} catch (const std::system_error &e) {
			/* if the server is getting flooded with a
			   large number of KILL comands, the
			   /proc/sys/net/unix/max_dgram_qlen limit may
			   be reached; wait a little bit before giving
			   up */
			if (IsErrno(e, EAGAIN)) {
				kill_queue.push_front({pid, signo});
				event.ScheduleWrite();
			} else
				throw;
		}
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

	case SpawnResponseCommand::MEMORY_WARNING:
		if (handler != nullptr) {
			// TODO: fix alignment
			const auto &p = *(const SpawnMemoryWarningPayload *)(const void *)payload.data;
			assert(payload.size == sizeof(p));
			handler->OnMemoryWarning(p.memory_usage, p.memory_max);
		}

		break;

	case SpawnResponseCommand::EXIT:
		HandleExitMessage(SpawnPayload(payload));
		break;
	}
}

inline void
SpawnServerClient::FlushKillQueue()
{
	while (!kill_queue.empty()) {
		const auto &i = kill_queue.front();

		SpawnSerializer s(SpawnRequestCommand::KILL);
		s.WriteInt(i.pid);
		s.WriteInt(i.signo);

		try {
			Send(s.GetPayload(), s.GetFds());
		} catch (const std::system_error &e) {
			if (IsErrno(e, EAGAIN))
				return;
			throw;
		}

		kill_queue.pop_front();
	}

	event.CancelWrite();
}

inline void
SpawnServerClient::ReceiveAndHandle()
{
	constexpr size_t N = 64;
	std::array<uint8_t[24], N> payloads;
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

	int n = recvmmsg(event.GetSocket().Get(), &msgs.front(), msgs.size(),
			 MSG_DONTWAIT|MSG_CMSG_CLOEXEC, nullptr);
	if (n <= 0) {
		if (n < 0)
			throw MakeErrno("recvmsg() from spawner failed");
		else
			throw std::runtime_error("spawner closed the socket");
	}

	for (int i = 0; i < n; ++i) {
		if (msgs[i].msg_len == 0)
			/* when the peer closes the socket, recvmmsg() doesn't
			   return 0; insteaed, it fills the mmsghdr array with
			   empty packets */
			throw std::runtime_error("spawner closed the socket");

		try {
			HandleMessage({payloads[i], msgs[i].msg_len});
		} catch (...) {
			PrintException(std::current_exception());
		}
	}
}

inline void
SpawnServerClient::OnSocketEvent(unsigned events) noexcept
try {
	if (events & event.ERROR)
		throw MakeErrno(event.GetSocket().GetError(),
				"Spawner socket error");

	if (events & event.HANGUP)
		throw std::runtime_error("Spawner hung up");

	if (events & event.WRITE)
		FlushKillQueue();

	if (events & event.READ)
		ReceiveAndHandle();
} catch (...) {
	PrintException(std::current_exception());
	Close();
}
