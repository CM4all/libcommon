// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Client.hxx"
#include "CgroupWatch.hxx"
#include "ProcessHandle.hxx"
#include "Handler.hxx"
#include "IProtocol.hxx"
#include "Builder.hxx"
#include "Parser.hxx"
#include "Prepared.hxx"
#include "CgroupOptions.hxx"
#include "Mount.hxx"
#include "ExitListener.hxx"
#include "system/Error.hxx"
#include "net/SocketPair.hxx"
#include "util/PrintException.hxx"

#include <stdexcept>

#include <assert.h>
#include <stdio.h>
#include <sys/signal.h>

static constexpr size_t MAX_FDS = 8;

struct SpawnServerClient::ChildProcess final
	: ChildProcessHandle, IntrusiveHashSetHook<IntrusiveHookMode::TRACK>
{
	SpawnServerClient &client;

	const unsigned pid;

	ExitListener *listener = nullptr;

	ChildProcess(SpawnServerClient &_client, unsigned _pid) noexcept
		:client(_client), pid(_pid) {}

	~ChildProcess() noexcept override {
		if (is_linked())
			Kill(SIGTERM);
	}

	void SetExitListener(ExitListener &_listener) noexcept override {
		assert(is_linked());

		listener = &_listener;
	}

	void Kill(int signo) noexcept override {
		assert(is_linked());

		client.Kill(*this, signo);
	}
};

inline unsigned
SpawnServerClient::ChildProcessGetKey::operator()(const ChildProcess &i) const noexcept
{
	return i.pid;
}

SpawnServerClient::SpawnServerClient(EventLoop &event_loop,
				     const SpawnConfig &_config,
				     UniqueSocketDescriptor _socket,
				     FileDescriptor cgroup,
				     bool _verify) noexcept
	:config(_config),
	 event(event_loop, BIND_THIS_METHOD(OnSocketEvent), _socket.Release()),
	 cgroups(cgroup.IsDefined()),
	 verify(_verify)
{
	event.ScheduleRead();

#ifdef HAVE_LIBSYSTEMD
	if (cgroup.IsDefined() &&
	    config.systemd_scope_properties.HaveMemoryLimit())
		cgroup_memory_watch = std::make_unique<CgroupMemoryWatch>(event_loop,
									  cgroup,
									  BIND_THIS_METHOD(OnCgroupMemoryWarning));
#endif
}

SpawnServerClient::~SpawnServerClient() noexcept
{
	event.Close();
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

#ifdef HAVE_LIBSYSTEMD
	cgroup_memory_watch.reset();
#endif

	ShutdownComplete();
}

bool
SpawnServerClient::ShutdownComplete() noexcept
{
	bool complete = shutting_down && processes.empty() && event.IsDefined();
	if (complete)
		Close();
	return complete;
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
SpawnServerClient::Send(std::span<const std::byte> payload,
			std::span<const FileDescriptor> fds)
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

	auto [local_socket, remote_socket] = CreateSocketPairNonBlock(AF_LOCAL, SOCK_SEQPACKET);

	static constexpr SpawnRequestCommand cmd = SpawnRequestCommand::CONNECT;

	const FileDescriptor remote_fd = remote_socket.ToFileDescriptor();

	try {
		const std::span payload{&cmd, 1};
		Send(std::as_bytes(payload), {&remote_fd, 1});
	} catch (...) {
		std::throw_with_nested(std::runtime_error("Spawn server failed"));
	}

	return std::move(local_socket);
}

static void
Serialize(SpawnSerializer &s, const CgroupOptions &c)
{
	s.WriteOptionalString(SpawnExecCommand::CGROUP, c.name);

	for (const auto &i : c.xattr) {
		s.Write(SpawnExecCommand::CGROUP_XATTR);
		s.WriteString(i.name);
		s.WriteString(i.value);
	}

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
	s.WriteOptionalString(SpawnExecCommand::PID_NS_NAME, ns.pid_namespace);
	s.WriteOptional(SpawnExecCommand::CGROUP_NS, ns.enable_cgroup);
	s.WriteOptional(SpawnExecCommand::NETWORK_NS, ns.enable_network);
	s.WriteOptionalString(SpawnExecCommand::NETWORK_NS_NAME,
			      ns.network_namespace);
	s.WriteOptional(SpawnExecCommand::IPC_NS, ns.enable_ipc);

	if (ns.mapped_uid > 0) {
		s.Write(SpawnExecCommand::MAPPED_UID);
		s.WriteT(ns.mapped_uid);
	}

	s.WriteOptional(SpawnExecCommand::MOUNT_PROC, ns.mount.mount_proc);
	s.WriteOptional(SpawnExecCommand::MOUNT_DEV, ns.mount.mount_dev);
	s.WriteOptional(SpawnExecCommand::MOUNT_PTS, ns.mount.mount_pts);
	s.WriteOptional(SpawnExecCommand::BIND_MOUNT_PTS,
			ns.mount.bind_mount_pts);
	s.WriteOptional(SpawnExecCommand::WRITABLE_PROC,
			ns.mount.writable_proc);
	s.WriteOptionalString(SpawnExecCommand::PIVOT_ROOT,
			      ns.mount.pivot_root);
	s.WriteOptional(SpawnExecCommand::MOUNT_ROOT_TMPFS,
			ns.mount.mount_root_tmpfs);

	s.WriteOptionalString(SpawnExecCommand::MOUNT_TMP_TMPFS,
			      ns.mount.mount_tmp_tmpfs);

	for (const auto &i : ns.mount.mounts) {
		switch (i.type) {
		case Mount::Type::BIND:
			if (i.source_fd.IsDefined()) {
				s.WriteFd(SpawnExecCommand::FD_BIND_MOUNT,
					  i.source_fd);
			} else {
				s.Write(SpawnExecCommand::BIND_MOUNT);
				s.WriteString(i.source);
			}

			s.WriteString(i.target);
			s.WriteBool(i.writable);
			s.WriteBool(i.exec);
			s.WriteBool(i.optional);
			break;

		case Mount::Type::BIND_FILE:
			if (i.source_fd.IsDefined()) {
				s.WriteFd(SpawnExecCommand::FD_BIND_MOUNT_FILE,
					  i.source_fd);
			} else {
				s.Write(SpawnExecCommand::BIND_MOUNT_FILE);
				s.WriteString(i.source);
			}

			s.WriteString(i.target);
			s.WriteBool(i.optional);
			break;

		case Mount::Type::TMPFS:
			s.WriteString(SpawnExecCommand::MOUNT_TMPFS,
				      i.target);
			s.WriteBool(i.writable);
			break;

		case Mount::Type::NAMED_TMPFS:
			s.Write(SpawnExecCommand::MOUNT_NAMED_TMPFS);
			s.WriteString(i.source);
			s.WriteString(i.target);
			s.WriteBool(i.writable);
			break;

		case Mount::Type::WRITE_FILE:
			s.WriteString(SpawnExecCommand::WRITE_FILE,
				      i.target);
			s.WriteString(i.source);
			s.WriteBool(i.optional);
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
	s.WriteU8(i);

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
	s.WriteU8(n_groups);
	for (size_t i = 0; i < n_groups; ++i)
		s.WriteT(uid_gid.groups[i]);
}

static void
Serialize(SpawnSerializer &s, const PreparedChildProcess &p)
{
	s.WriteOptionalString(SpawnExecCommand::HOOK_INFO, p.hook_info);

	if (p.exec_function != nullptr) {
		s.Write(SpawnExecCommand::EXEC_FUNCTION);
		s.WriteT(p.exec_function);
	}

	if (p.exec_fd.IsDefined())
		s.WriteFd(SpawnExecCommand::EXEC_FD, p.exec_fd);
	else
		s.WriteOptionalString(SpawnExecCommand::EXEC_PATH, p.exec_path);

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

	if (p.stdout_fd.IsDefined()) {
		if (p.stdout_fd == p.stdin_fd)
			s.Write(SpawnExecCommand::STDOUT_IS_STDIN);
		else
			s.WriteFd(SpawnExecCommand::STDOUT, p.stdout_fd);
	}

	if (p.stderr_fd.IsDefined()) {
		if (p.stderr_fd == p.stdin_fd)
			s.Write(SpawnExecCommand::STDERR_IS_STDIN);
		else
			s.WriteFd(SpawnExecCommand::STDERR, p.stderr_fd);
	}

	s.CheckWriteFd(SpawnExecCommand::CONTROL, p.control_fd);

	s.CheckWriteFd(SpawnExecCommand::RETURN_STDERR,
		       p.return_stderr.ToFileDescriptor());

	s.CheckWriteFd(SpawnExecCommand::RETURN_PIDFD,
		       p.return_pidfd.ToFileDescriptor());

	s.CheckWriteFd(SpawnExecCommand::RETURN_CGROUP,
		       p.return_cgroup.ToFileDescriptor());

	s.WriteOptionalString(SpawnExecCommand::STDERR_PATH, p.stderr_path);

	if (p.priority != 0) {
		s.Write(SpawnExecCommand::PRIORITY);
		s.WriteInt(p.priority);
	}

	if (p.cgroup != nullptr) {
		Serialize(s, *p.cgroup);
		s.WriteOptionalString(SpawnExecCommand::CGROUP_SESSION,
				      p.cgroup_session);
	}

	Serialize(s, p.ns);
	Serialize(s, p.rlimits);
	Serialize(s, p.uid_gid);

	s.WriteOptionalString(SpawnExecCommand::CHROOT, p.chroot);
	s.WriteOptionalString(SpawnExecCommand::CHDIR, p.chdir);

	if (p.sched_idle)
		s.Write(SpawnExecCommand::SCHED_IDLE_);

	if (p.ioprio_idle)
		s.Write(SpawnExecCommand::IOPRIO_IDLE);

#ifdef HAVE_LIBSECCOMP
	if (p.forbid_user_ns)
		s.Write(SpawnExecCommand::FORBID_USER_NS);

	if (p.forbid_multicast)
		s.Write(SpawnExecCommand::FORBID_MULTICAST);

	if (p.forbid_bind)
		s.Write(SpawnExecCommand::FORBID_BIND);
#endif // HAVE_LIBSECCOMP

	if (p.no_new_privs)
		s.Write(SpawnExecCommand::NO_NEW_PRIVS);

	if (p.tty)
		s.Write(SpawnExecCommand::TTY);
}

std::unique_ptr<ChildProcessHandle>
SpawnServerClient::SpawnChildProcess(const char *name,
				     PreparedChildProcess &&p)
{
	assert(!shutting_down);

	/* this check is performed again on the server (which is obviously
	   necessary, and the only way to have it secure); this one is
	   only here for the developer to see the error earlier in the
	   call chain */
	if (verify && !p.uid_gid.IsEmpty())
		config.Verify(p.uid_gid);

	CheckOrAbort();

	const unsigned pid = MakePid();

	SpawnSerializer s(SpawnRequestCommand::EXEC);

	try {
		s.WriteUnsigned(pid);
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

	auto handle = std::make_unique<ChildProcess>(*this, pid);
	processes.insert(*handle);
	return handle;
}

void
SpawnServerClient::Kill(ChildProcess &child, int signo) noexcept
{
	CheckOrAbort();

	assert(child.is_linked());
	processes.erase(processes.iterator_to(child));

	if (ShutdownComplete())
		return;

	if (kill_queue.empty())
		event.ScheduleWrite();

	kill_queue.push_front({child.pid, signo});
}

inline void
SpawnServerClient::HandleOneExit(SpawnPayload &payload)
{
	unsigned pid;
	int status;
	payload.ReadUnsigned(pid);
	payload.ReadInt(status);

	auto i = processes.find(pid);
	if (i == processes.end())
		return;

	processes.erase(i);

	if (i->listener != nullptr)
		i->listener->OnChildProcessExit(status);
}

inline void
SpawnServerClient::HandleExitMessage(SpawnPayload payload)
{
	while (!payload.empty())
		HandleOneExit(payload);

	ShutdownComplete();
}

inline void
SpawnServerClient::HandleMessage(std::span<const std::byte> payload,
				 [[maybe_unused]] std::span<UniqueFileDescriptor> fds)
{
	const auto cmd = (SpawnResponseCommand)payload.front();
	payload = payload.subspan(1);

	switch (cmd) {
	case SpawnResponseCommand::EXIT:
		HandleExitMessage(SpawnPayload(payload));
		break;
	}
}

inline void
SpawnServerClient::FlushKillQueue()
{
	if (kill_queue.empty())
		return;

	SpawnSerializer s{SpawnRequestCommand::KILL};

	for (std::size_t n = 0; n < 256 && !kill_queue.empty(); ++n) {
		const auto &i = kill_queue.front();

		s.WriteUnsigned(i.pid);
		s.WriteInt(i.signo);

		kill_queue.pop_front();
	}

	Send(s.GetPayload(), s.GetFds());
}

inline void
SpawnServerClient::ReceiveAndHandle()
{
	if (!receive.Receive(event.GetSocket()))
		throw std::runtime_error("spawner closed the socket");

	for (const auto &i : receive) {
		if (i.payload.empty())
			/* when the peer closes the socket, recvmmsg() doesn't
			   return 0; insteaed, it fills the mmsghdr array with
			   empty packets */
			throw std::runtime_error("spawner closed the socket");

		try {
			HandleMessage(i.payload, i.fds);
		} catch (...) {
			PrintException(std::current_exception());
		}
	}

	receive.Clear();
}

inline void
SpawnServerClient::OnSocketEvent(unsigned events) noexcept
try {
	if (events & event.ERROR)
		throw MakeErrno(event.GetSocket().GetError(),
				"Spawner socket error");

	if (events & event.HANGUP)
		throw std::runtime_error("Spawner hung up");

	if (events & event.WRITE) {
		FlushKillQueue();

		if (kill_queue.empty())
			event.CancelWrite();
	}

	if (events & event.READ)
		ReceiveAndHandle();
} catch (...) {
	fprintf(stderr, "Spawner error: ");
	PrintException(std::current_exception());
	Close();
}

uint_least64_t
SpawnServerClient::GetMemoryUsage() const
{
#ifdef HAVE_LIBSYSTEMD
	if (cgroup_memory_watch)
		return cgroup_memory_watch->GetMemoryUsage();
#endif

	return 0;
}

#ifdef HAVE_LIBSYSTEMD

inline void
SpawnServerClient::OnCgroupMemoryWarning(uint_least64_t memory_usage) noexcept
{
	if (handler != nullptr)
		handler->OnMemoryWarning(memory_usage,
					 config.systemd_scope_properties.memory_high,
					 config.systemd_scope_properties.memory_max);
}

#endif
