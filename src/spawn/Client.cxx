// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Client.hxx"
#include "ProcessHandle.hxx"
#include "CompletionHandler.hxx"
#include "IProtocol.hxx"
#include "Builder.hxx"
#include "Parser.hxx"
#include "Prepared.hxx"
#include "CgroupOptions.hxx"
#include "Mount.hxx"
#include "ExitListener.hxx"
#include "lib/fmt/ExceptionFormatter.hxx"
#include "net/SocketError.hxx"
#include "net/SocketPair.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "util/Cancellable.hxx"

#include <fmt/core.h>

#include <stdexcept>

#include <assert.h>
#include <stdio.h>
#include <sys/signal.h>

using namespace Spawn;

static constexpr size_t MAX_FDS = 8;

class SpawnServerClient::SpawnQueueItem final
	: public IntrusiveListHook<>, Cancellable
{
	const EnqueueCallback callback;

public:
	SpawnQueueItem(EnqueueCallback _callback,
		       CancellablePointer &cancel_ptr) noexcept
		:callback(_callback)
	{
		cancel_ptr = *this;
	}

	void RemoveAndInvoke() noexcept {
		unlink();
		auto _callback = callback;
		delete this;
		_callback();
	}

private:
	void Cancel() noexcept override {
		unlink();
		delete this;
	}
};

struct SpawnServerClient::ChildProcess final
	: ChildProcessHandle, IntrusiveHashSetHook<IntrusiveHookMode::TRACK>
{
	SpawnServerClient &client;

	const unsigned pid;

	SpawnCompletionHandler *completion_handler = nullptr;

	ExitListener *listener = nullptr;

	ChildProcess(SpawnServerClient &_client, unsigned _pid) noexcept
		:client(_client), pid(_pid) {}

	~ChildProcess() noexcept override {
		if (is_linked())
			Kill(SIGTERM);
	}

	/* virtual methods from class ChildProcessHandle */
	void SetCompletionHandler(SpawnCompletionHandler &_completion_handler) noexcept override {
		assert(!completion_handler);

		completion_handler = &_completion_handler;
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
				     bool _cgroups,
				     bool _verify) noexcept
	:config(_config),
	 event(event_loop, BIND_THIS_METHOD(OnSocketEvent), _socket.Release()),
	 defer_spawn_queue(event_loop, BIND_THIS_METHOD(OnDeferredSpawnQueue)),
	 cgroups(_cgroups),
	 verify(_verify)
{
	event.ScheduleRead();
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
		fmt::print(stderr, "SpawnChildProcess: the spawner is gone, emergency!\n");
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
SpawnServerClient::Send(const Serializer &s)
{
	::Send<MAX_FDS>(event.GetSocket(), s);
}

UniqueSocketDescriptor
SpawnServerClient::Connect()
{
	CheckOrAbort();

	auto [local_socket, remote_socket] = CreateSocketPairNonBlock(AF_LOCAL, SOCK_SEQPACKET);

	static constexpr RequestCommand cmd = RequestCommand::CONNECT;

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
Serialize(Serializer &s, const CgroupOptions &c)
{
	s.WriteOptionalString(ExecCommand::CGROUP, c.name);

	for (const auto &i : c.xattr) {
		s.Write(ExecCommand::CGROUP_XATTR);
		s.WriteString(i.name);
		s.WriteString(i.value);
	}

	for (const auto &i : c.set) {
		s.Write(ExecCommand::CGROUP_SET);
		s.WriteString(i.name);
		s.WriteString(i.value);
	}
}

static void
Serialize(Serializer &s, const NamespaceOptions &ns)
{
	s.WriteOptional(ExecCommand::USER_NS, ns.enable_user);
	s.WriteOptional(ExecCommand::PID_NS, ns.enable_pid);
	s.WriteOptionalString(ExecCommand::PID_NS_NAME, ns.pid_namespace);
	s.WriteOptional(ExecCommand::CGROUP_NS, ns.enable_cgroup);
	s.WriteOptional(ExecCommand::NETWORK_NS, ns.enable_network);
	s.WriteOptionalString(ExecCommand::NETWORK_NS_NAME,
			      ns.network_namespace);
	s.WriteOptional(ExecCommand::IPC_NS, ns.enable_ipc);

	if (ns.mapped_uid > 0) {
		s.Write(ExecCommand::MAPPED_UID);
		s.WriteT(ns.mapped_uid);
	}

	s.WriteOptional(ExecCommand::MOUNT_PROC, ns.mount.mount_proc);
	s.WriteOptional(ExecCommand::MOUNT_DEV, ns.mount.mount_dev);
	s.WriteOptional(ExecCommand::MOUNT_PTS, ns.mount.mount_pts);
	s.WriteOptional(ExecCommand::BIND_MOUNT_PTS,
			ns.mount.bind_mount_pts);
	s.WriteOptional(ExecCommand::WRITABLE_PROC,
			ns.mount.writable_proc);
	s.WriteOptionalString(ExecCommand::PIVOT_ROOT,
			      ns.mount.pivot_root);
	s.WriteOptional(ExecCommand::MOUNT_ROOT_TMPFS,
			ns.mount.mount_root_tmpfs);

	s.WriteOptionalString(ExecCommand::MOUNT_TMP_TMPFS,
			      ns.mount.mount_tmp_tmpfs);

	for (const auto &i : ns.mount.mounts) {
		switch (i.type) {
		case Mount::Type::BIND:
			if (i.source_fd.IsDefined()) {
				s.WriteFd(ExecCommand::FD_BIND_MOUNT,
					  i.source_fd);
			} else {
				s.Write(ExecCommand::BIND_MOUNT);
				s.WriteString(i.source);
			}

			s.WriteString(i.target);
			s.WriteBool(i.writable);
			s.WriteBool(i.exec);
			s.WriteBool(i.optional);
			break;

		case Mount::Type::BIND_FILE:
			if (i.source_fd.IsDefined()) {
				s.WriteFd(ExecCommand::FD_BIND_MOUNT_FILE,
					  i.source_fd);
			} else {
				s.Write(ExecCommand::BIND_MOUNT_FILE);
				s.WriteString(i.source);
			}

			s.WriteString(i.target);
			s.WriteBool(i.exec);
			s.WriteBool(i.optional);
			break;

		case Mount::Type::TMPFS:
			s.WriteString(ExecCommand::MOUNT_TMPFS,
				      i.target);
			s.WriteBool(i.writable);
			break;

		case Mount::Type::NAMED_TMPFS:
			s.Write(ExecCommand::MOUNT_NAMED_TMPFS);
			s.WriteString(i.source);
			s.WriteString(i.target);
			s.WriteBool(i.writable);
			break;

		case Mount::Type::WRITE_FILE:
			s.WriteString(ExecCommand::WRITE_FILE,
				      i.target);
			s.WriteString(i.source);
			s.WriteBool(i.optional);
			break;

		case Mount::Type::SYMLINK:
			s.WriteString(ExecCommand::SYMLINK, i.target);
			s.WriteString(i.source);
			break;
		}
	}

	if (ns.mount.dir_mode != 0711) {
		s.Write(ExecCommand::DIR_MODE);
		s.WriteT(ns.mount.dir_mode);
	}

	s.WriteOptionalString(ExecCommand::HOSTNAME, ns.hostname);
}

static void
Serialize(Serializer &s, unsigned i, const ResourceLimit &rlimit)
{
	if (rlimit.IsEmpty())
		return;

	s.Write(ExecCommand::RLIMIT);
	s.WriteU8(i);

	const struct rlimit &data = rlimit;
	s.WriteT(data);
}

static void
Serialize(Serializer &s, const ResourceLimits &rlimits)
{
	for (unsigned i = 0; i < RLIM_NLIMITS; ++i)
		Serialize(s, i, rlimits.values[i]);
}

static void
Serialize(Serializer &s, const UidGid &uid_gid)
{
	if (uid_gid.IsEmpty())
		return;

	s.Write(ExecCommand::UID_GID);
	s.WriteT(uid_gid.real_uid);
	s.WriteT(uid_gid.real_gid);
	s.WriteT(uid_gid.effective_uid);
	s.WriteT(uid_gid.effective_gid);

	const size_t n_groups = uid_gid.CountSupplementaryGroups();
	s.WriteU8(n_groups);
	for (size_t i = 0; i < n_groups; ++i)
		s.WriteT(uid_gid.supplementary_groups[i]);
}

static void
Serialize(Serializer &s, const PreparedChildProcess &p)
{
	s.WriteOptionalString(ExecCommand::HOOK_INFO, p.hook_info);

	if (p.exec_function != nullptr) {
		s.Write(ExecCommand::EXEC_FUNCTION);
		s.WriteT(p.exec_function);
	}

	if (p.exec_fd.IsDefined())
		s.WriteFd(ExecCommand::EXEC_FD, p.exec_fd);
	else
		s.WriteOptionalString(ExecCommand::EXEC_PATH, p.exec_path);

	for (const char *i : p.args)
		s.WriteString(ExecCommand::ARG, i);

	for (const char *i : p.env)
		s.WriteString(ExecCommand::SETENV, i);

	if (p.umask >= 0) {
		uint16_t umask = p.umask;
		s.Write(ExecCommand::UMASK);
		s.WriteT(umask);
	}

	s.CheckWriteFd(ExecCommand::STDIN, p.stdin_fd);

	if (p.stdout_fd.IsDefined()) {
		if (p.stdout_fd == p.stdin_fd)
			s.Write(ExecCommand::STDOUT_IS_STDIN);
		else
			s.WriteFd(ExecCommand::STDOUT, p.stdout_fd);
	}

	if (p.stderr_fd.IsDefined()) {
		if (p.stderr_fd == p.stdin_fd)
			s.Write(ExecCommand::STDERR_IS_STDIN);
		else
			s.WriteFd(ExecCommand::STDERR, p.stderr_fd);
	}

	s.CheckWriteFd(ExecCommand::CONTROL, p.control_fd);

	s.CheckWriteFd(ExecCommand::RETURN_STDERR,
		       p.return_stderr.ToFileDescriptor());

	s.CheckWriteFd(ExecCommand::RETURN_PIDFD,
		       p.return_pidfd.ToFileDescriptor());

	s.CheckWriteFd(ExecCommand::RETURN_CGROUP,
		       p.return_cgroup.ToFileDescriptor());

	s.WriteOptionalString(ExecCommand::STDERR_PATH, p.stderr_path);

	if (p.priority != 0) {
		s.Write(ExecCommand::PRIORITY);
		s.WriteInt(p.priority);
	}

	if (p.cgroup != nullptr) {
		Serialize(s, *p.cgroup);
		s.WriteOptionalString(ExecCommand::CGROUP_SESSION,
				      p.cgroup_session);
	}

	Serialize(s, p.ns);
	Serialize(s, p.rlimits);
	Serialize(s, p.uid_gid);

	s.WriteOptionalString(ExecCommand::CHROOT, p.chroot);
	s.WriteOptionalString(ExecCommand::CHDIR, p.chdir);

	if (p.sched_idle)
		s.Write(ExecCommand::SCHED_IDLE_);

	if (p.ioprio_idle)
		s.Write(ExecCommand::IOPRIO_IDLE);

#ifdef HAVE_LIBSECCOMP
	if (p.forbid_user_ns)
		s.Write(ExecCommand::FORBID_USER_NS);

	if (p.forbid_multicast)
		s.Write(ExecCommand::FORBID_MULTICAST);

	if (p.forbid_bind)
		s.Write(ExecCommand::FORBID_BIND);
#endif // HAVE_LIBSECCOMP

#ifdef HAVE_LIBCAP
	if (p.cap_sys_resource)
		s.Write(ExecCommand::CAP_SYS_RESOURCE);
#endif // HAVE_LIBCAP

	if (p.no_new_privs)
		s.Write(ExecCommand::NO_NEW_PRIVS);

	if (p.tty)
		s.Write(ExecCommand::TTY);
}

std::unique_ptr<ChildProcessHandle>
SpawnServerClient::SpawnChildProcess(std::string_view name,
				     PreparedChildProcess &&p)
try {
	assert(!shutting_down);

	++stats.spawned;

	/* this check is performed again on the server (which is obviously
	   necessary, and the only way to have it secure); this one is
	   only here for the developer to see the error earlier in the
	   call chain */
	if (verify && !p.uid_gid.IsEmpty())
		config.Verify(p.uid_gid);

	CheckOrAbort();

	const unsigned pid = MakePid();

	Serializer s{RequestCommand::EXEC};

	try {
		s.WriteUnsigned(pid);
		s.WriteString(name);

		Serialize(s, p);
	} catch (PayloadTooLargeError) {
		throw std::runtime_error("Spawn payload is too large");
	}

	try {
		Send(s.GetPayload(), s.GetFds());
	} catch (const std::runtime_error &e) {
		std::throw_with_nested(std::runtime_error("Spawn server failed"));
	}

	++n_pending_execs;
	if (IsUnderPressure())
		defer_spawn_queue.Cancel();

	auto handle = std::make_unique<ChildProcess>(*this, pid);
	processes.insert(*handle);
	return handle;
} catch (...) {
	++stats.errors;
	throw;
}

void
SpawnServerClient::Enqueue(EnqueueCallback callback, CancellablePointer &cancel_ptr) noexcept
{
	if (IsUnderPressure()) {
		auto *item = new SpawnQueueItem(callback, cancel_ptr);
		spawn_queue.push_back(*item);
	} else
		callback();
}

void
SpawnServerClient::OnDeferredSpawnQueue() noexcept
{
	assert(!IsUnderPressure());

	if (spawn_queue.empty())
		return;

	spawn_queue.front().RemoveAndInvoke();

	if (!IsUnderPressure() && !spawn_queue.empty())
		defer_spawn_queue.Schedule();
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

	++stats.killed;
}

inline void
SpawnServerClient::HandleExecCompleteMessage(Payload payload)
{
	assert(!payload.empty());

	while (!payload.empty()) {
		assert(n_pending_execs > 0);
		--n_pending_execs;

		unsigned pid;
		payload.ReadUnsigned(pid);
		const char *error = payload.ReadString();

		if (const auto i = processes.find(pid); i != processes.end()) {
			// TODO forward errors
			if (i->completion_handler) {
				if (*error == 0) {
					i->completion_handler->OnSpawnSuccess();
				} else {
					++stats.errors;
					i->completion_handler->OnSpawnError(std::make_exception_ptr(std::runtime_error{error}));
				}

				/* if there is a completion handler,
				   don't log error message to
				   stderr */
				continue;
			}
		}

		if (*error != 0)
			fmt::print(stderr, "Failed to spawn child process {}: {}\n",
					   pid, error);
	}

	if (!IsUnderPressure() && !spawn_queue.empty())
		defer_spawn_queue.Schedule();
}

inline void
SpawnServerClient::HandleOneExit(Payload &payload)
{
	unsigned pid;
	int status;
	payload.ReadUnsigned(pid);
	payload.ReadInt(status);

	++stats.exited;

	auto i = processes.find(pid);
	if (i == processes.end())
		return;

	processes.erase(i);

	if (i->listener != nullptr)
		i->listener->OnChildProcessExit(status);
}

inline void
SpawnServerClient::HandleExitMessage(Payload payload)
{
	while (!payload.empty())
		HandleOneExit(payload);

	ShutdownComplete();
}

inline void
SpawnServerClient::HandleMessage(std::span<const std::byte> payload,
				 [[maybe_unused]] std::span<UniqueFileDescriptor> fds)
{
	const auto cmd = static_cast<ResponseCommand>(payload.front());
	payload = payload.subspan(1);

	switch (cmd) {
	case ResponseCommand::EXEC_COMPLETE:
		HandleExecCompleteMessage(Payload{payload});
		break;

	case ResponseCommand::EXIT:
		HandleExitMessage(Payload(payload));
		break;
	}
}

inline void
SpawnServerClient::FlushKillQueue()
{
	if (kill_queue.empty())
		return;

	Serializer s{RequestCommand::KILL};

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
			fmt::print(stderr, "{}\n", std::current_exception());
		}
	}

	receive.Clear();
}

inline void
SpawnServerClient::OnSocketEvent(unsigned events) noexcept
try {
	if (events & event.ERROR)
		throw MakeSocketError(event.GetSocket().GetError(),
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
	fmt::print(stderr, "Spawner error: {}\n", std::current_exception());
	Close();
}
