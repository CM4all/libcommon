// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Server.hxx"
#include "Config.hxx"
#include "IProtocol.hxx"
#include "Parser.hxx"
#include "Builder.hxx"
#include "Prepared.hxx"
#include "CgroupWatch.hxx"
#include "CgroupOptions.hxx"
#include "Hook.hxx"
#include "Mount.hxx"
#include "CgroupState.hxx"
#include "Direct.hxx"
#include "Registry.hxx"
#include "ZombieReaper.hxx"
#include "ExitListener.hxx"
#include "PidfdEvent.hxx"
#include "lib/cap/Glue.hxx"
#include "event/SocketEvent.hxx"
#include "event/Loop.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "net/ReceiveMessage.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "util/DeleteDisposer.hxx"
#include "util/IntrusiveHashSet.hxx"
#include "util/IntrusiveList.hxx"
#include "util/PrintException.hxx"
#include "util/Exception.hxx"

#include <forward_list>
#include <memory>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>

class SpawnServerProcess;

class SpawnFdList {
	std::vector<UniqueFileDescriptor> v;
	decltype(v)::iterator i;

public:
	SpawnFdList(std::vector<UniqueFileDescriptor> &&_v) noexcept
		:v(std::move(_v)),
		 i(v.begin()) {}

	SpawnFdList(SpawnFdList &&src) = default;

	SpawnFdList &operator=(SpawnFdList &&src) = default;

	bool IsEmpty() noexcept {
		return i == v.end();
	}

	size_t size() const noexcept {
		return v.size();
	}

	UniqueFileDescriptor Get() {
		if (IsEmpty())
			throw MalformedSpawnPayloadError();

		return std::move(*i++);
	}

	UniqueSocketDescriptor GetSocket() {
		return UniqueSocketDescriptor{Get().Release()};
	}
};

class SpawnServerConnection;

class SpawnServerChild final : public ExitListener,
			       public IntrusiveHashSetHook<IntrusiveHookMode::NORMAL>
{
	SpawnServerConnection &connection;

	const unsigned id;

	std::unique_ptr<PidfdEvent> pidfd;

	const std::string name;

public:
	explicit SpawnServerChild(EventLoop &event_loop,
				  SpawnServerConnection &_connection,
				  unsigned _id, UniqueFileDescriptor _pidfd,
				  const char *_name) noexcept
		:connection(_connection), id(_id),
		 pidfd(std::make_unique<PidfdEvent>(event_loop,
						    std::move(_pidfd),
						    _name,
						    (ExitListener &)*this)),
		 name(_name) {}

	SpawnServerChild(const SpawnServerChild &) = delete;
	SpawnServerChild &operator=(const SpawnServerChild &) = delete;

	const char *GetName() const noexcept {
		return name.c_str();
	}

	void Kill(ChildProcessRegistry &child_process_registry,
		  int signo) noexcept {
		child_process_registry.Kill(std::move(pidfd), signo);
	}

	/* virtual methods from ExitListener */
	void OnChildProcessExit(int status) noexcept override;

	struct Hash {
		constexpr std::size_t operator()(unsigned i) const noexcept {
			return i;
		}

		std::size_t operator()(const SpawnServerChild &i) const noexcept {
			return i.id;
		}
	};

	struct Equal {
		bool operator()(const unsigned a,
				const SpawnServerChild &b) const noexcept {
			return a == b.id;
		}
	};
};

class SpawnServerConnection final
	: public IntrusiveListHook<IntrusiveHookMode::NORMAL>
{
	SpawnServerProcess &process;
	UniqueSocketDescriptor socket;

	const LLogger logger;

	SocketEvent event;

	using ChildIdMap = IntrusiveHashSet<SpawnServerChild, 1021,
					    SpawnServerChild::Hash,
					    SpawnServerChild::Equal>;
	ChildIdMap children;

	struct ExitQueueItem {
		unsigned id;
		int status;
	};

	/**
	 * Filled by SendExit() if sendmsg()==EAGAIN.
	 */
	std::forward_list<ExitQueueItem> exit_queue;

public:
	SpawnServerConnection(SpawnServerProcess &_process,
			      UniqueSocketDescriptor &&_socket) noexcept;
	~SpawnServerConnection() noexcept;

	auto &GetEventLoop() const noexcept {
		return event.GetEventLoop();
	}

	void OnChildProcessExit(unsigned id, int status,
				SpawnServerChild *child) noexcept;

#ifdef HAVE_LIBSYSTEMD
	void SendMemoryWarning(uint64_t memory_usage,
			       uint64_t memory_max) noexcept;
#endif

private:
	void RemoveConnection() noexcept;

	void SendExit(unsigned id, int status) noexcept;
	void SpawnChild(unsigned id, const char *name,
			PreparedChildProcess &&p) noexcept;

	void HandleExecMessage(SpawnPayload payload, SpawnFdList &&fds);
	void HandleOneKill(SpawnPayload &payload);
	void HandleKillMessage(SpawnPayload payload, SpawnFdList &&fds);
	void HandleMessage(std::span<const std::byte> payload, SpawnFdList &&fds);
	void HandleMessage(ReceiveMessageResult &&result);

	void ReceiveAndHandle();
	void FlushExitQueue();

	void OnSocketEvent(unsigned events) noexcept;
};

void
SpawnServerChild::OnChildProcessExit(int status) noexcept
{
	connection.OnChildProcessExit(id, status, this);
}

void
SpawnServerConnection::OnChildProcessExit(unsigned id, int status,
					  SpawnServerChild *child) noexcept
{
	children.erase(children.iterator_to(*child));
	delete child;

	SendExit(id, status);
}

class SpawnServerProcess {
	const SpawnConfig config;
	const CgroupState &cgroup_state;
	SpawnHook *const hook;

	const LLogger logger;

	EventLoop loop;

	ChildProcessRegistry child_process_registry;

	ZombieReaper zombie_reaper{loop};

#ifdef HAVE_LIBSYSTEMD
	std::unique_ptr<CgroupMemoryWatch> cgroup_memory_watch;
#endif

	using ConnectionList = IntrusiveList<SpawnServerConnection>;
	ConnectionList connections;

	const bool is_sys_admin = ::IsSysAdmin();

public:
	SpawnServerProcess(const SpawnConfig &_config,
			   const CgroupState &_cgroup_state,
			   SpawnHook *_hook)
		:config(_config), cgroup_state(_cgroup_state), hook(_hook),
		 logger("spawn")
	{
#ifdef HAVE_LIBSYSTEMD
		if (config.systemd_scope_properties.memory_max > 0 &&
		    cgroup_state.IsEnabled())
			cgroup_memory_watch = std::make_unique<CgroupMemoryWatch>
				(loop, cgroup_state,
				 BIND_THIS_METHOD(OnCgroupMemoryWarning));
#endif
	}

	const SpawnConfig &GetConfig() const noexcept {
		return config;
	}

	const CgroupState &GetCgroupState() const noexcept {
		return cgroup_state;
	}

	bool IsSysAdmin() const noexcept {
		return is_sys_admin;
	}

	EventLoop &GetEventLoop() noexcept {
		return loop;
	}

	ChildProcessRegistry &GetChildProcessRegistry() noexcept {
		return child_process_registry;
	}

	bool Verify(const PreparedChildProcess &p) const {
		return hook != nullptr && hook->Verify(p);
	}

	void AddConnection(UniqueSocketDescriptor &&_socket) noexcept {
		auto connection = new SpawnServerConnection(*this, std::move(_socket));
		connections.push_back(*connection);
	}

	void RemoveConnection(SpawnServerConnection &connection) noexcept {
		connections.erase_and_dispose(connections.iterator_to(connection),
					      DeleteDisposer());

		if (connections.empty())
			/* all connections are gone */
			Quit();
	}

	void Run() noexcept {
		loop.Run();
	}

private:
	void Quit() noexcept {
		assert(connections.empty());

#ifdef HAVE_LIBSYSTEMD
		cgroup_memory_watch.reset();
#endif

		zombie_reaper.Disable();
	}

#ifdef HAVE_LIBSYSTEMD
	void OnCgroupMemoryWarning(uint64_t memory_usage) noexcept {
		for (auto &c : connections)
			c.SendMemoryWarning(memory_usage,
					    config.systemd_scope_properties.memory_max);
	}
#endif
};

SpawnServerConnection::SpawnServerConnection(SpawnServerProcess &_process,
					     UniqueSocketDescriptor &&_socket) noexcept
	:process(_process), socket(std::move(_socket)),
	 logger("spawn"),
	 event(process.GetEventLoop(), BIND_THIS_METHOD(OnSocketEvent),
	       socket)
{
	event.ScheduleRead();
}

SpawnServerConnection::~SpawnServerConnection() noexcept
{
	event.Cancel();

	auto &registry = process.GetChildProcessRegistry();
	children.clear_and_dispose([&registry](SpawnServerChild *child){
			child->Kill(registry, SIGTERM);
			delete child;
		});
}

inline void
SpawnServerConnection::RemoveConnection() noexcept
{
	process.RemoveConnection(*this);
}

#ifdef HAVE_LIBSYSTEMD

void
SpawnServerConnection::SendMemoryWarning(uint64_t memory_usage,
					 uint64_t memory_max) noexcept
{
	SpawnSerializer s(SpawnResponseCommand::MEMORY_WARNING);
	s.WriteT(SpawnMemoryWarningPayload{memory_usage, memory_max});

	try {
		::Send<1>(socket, s);
	} catch (...) {
		logger(1, "Failed to send MEMORY_WARNING to worker: ",
		       GetFullMessage(std::current_exception()).c_str());
	}
}

#endif

void
SpawnServerConnection::SendExit(unsigned id, int status) noexcept
{
	if (exit_queue.empty())
		event.ScheduleWrite();

	exit_queue.push_front({id, status});
}

inline void
SpawnServerConnection::SpawnChild(unsigned id, const char *name,
				  PreparedChildProcess &&p) noexcept
{
	const auto &config = process.GetConfig();

	if (!p.uid_gid.IsEmpty()) {
		try {
			if (!process.Verify(p))
				config.Verify(p.uid_gid);
		} catch (const std::exception &e) {
			PrintException(e);
			SendExit(id, W_EXITCODE(0xff, 0));
			return;
		}
	}

	if (p.uid_gid.IsEmpty()) {
		if (config.default_uid_gid.IsEmpty()) {
			logger(1, "No uid/gid specified");
			SendExit(id, W_EXITCODE(0xff, 0));
			return;
		}

		p.uid_gid = config.default_uid_gid;
	}

	UniqueFileDescriptor pid;

	try {
		pid = SpawnChildProcess(std::move(p),
					process.GetCgroupState(),
					process.IsSysAdmin()).first;
	} catch (...) {
		logger(1, "Failed to spawn child process: ",
		       GetFullMessage(std::current_exception()).c_str());
		SendExit(id, W_EXITCODE(0xff, 0));
		return;
	}

	auto *child = new SpawnServerChild(GetEventLoop(), *this, id,
					   std::move(pid), name);
	children.insert(*child);
}

static void
Read(SpawnPayload &payload, ResourceLimits &rlimits)
{
	unsigned i = (unsigned)payload.ReadByte();
	struct rlimit &data = rlimits.values[i];
	payload.ReadT(data);
}

static void
Read(SpawnPayload &payload, UidGid &uid_gid)
{
	payload.ReadT(uid_gid.uid);
	payload.ReadT(uid_gid.gid);

	const size_t n_groups = (std::size_t)payload.ReadByte();
	if (n_groups > uid_gid.groups.max_size())
		throw MalformedSpawnPayloadError();

	for (size_t i = 0; i < n_groups; ++i)
		payload.ReadT(uid_gid.groups[i]);

	if (n_groups < uid_gid.groups.max_size())
		uid_gid.groups[n_groups] = 0;
}

inline void
SpawnServerConnection::HandleExecMessage(SpawnPayload payload,
					 SpawnFdList &&fds)
{
	unsigned id;
	payload.ReadUnsigned(id);
	const char *name = payload.ReadString();

	PreparedChildProcess p;
	CgroupOptions cgroup;

	auto mount_tail = p.ns.mount.mounts.before_begin();

	std::forward_list<Mount> mounts;
	std::forward_list<std::string> strings;
	std::forward_list<AssignmentList::Item> assignments;

	while (!payload.empty()) {
		const SpawnExecCommand cmd = (SpawnExecCommand)payload.ReadByte();
		switch (cmd) {
		case SpawnExecCommand::ARG:
			if (p.args.size() >= 16384)
				throw MalformedSpawnPayloadError();

			p.Append(payload.ReadString());
			break;

		case SpawnExecCommand::SETENV:
			if (p.env.size() >= 16384)
				throw MalformedSpawnPayloadError();

			p.PutEnv(payload.ReadString());
			break;

		case SpawnExecCommand::UMASK:
			{
				uint16_t value;
				payload.ReadT(value);
				p.umask = value;
			}

			break;

		case SpawnExecCommand::STDIN:
			p.SetStdin(fds.Get().Steal());
			break;

		case SpawnExecCommand::STDOUT:
			p.SetStdout(fds.Get().Steal());
			break;

		case SpawnExecCommand::STDERR:
			p.SetStderr(fds.Get().Steal());
			break;

		case SpawnExecCommand::STDERR_PATH:
			p.stderr_path = payload.ReadString();
			break;

		case SpawnExecCommand::RETURN_STDERR:
			p.return_stderr = UniqueSocketDescriptor{fds.Get().Release()};
			break;

		case SpawnExecCommand::RETURN_PIDFD:
			p.return_pidfd = UniqueSocketDescriptor{fds.Get().Release()};
			break;

		case SpawnExecCommand::RETURN_CGROUP:
			p.return_cgroup = UniqueSocketDescriptor{fds.Get().Release()};
			break;

		case SpawnExecCommand::CONTROL:
			p.SetControl(fds.Get());
			break;

		case SpawnExecCommand::TTY:
			p.tty = true;
			break;

		case SpawnExecCommand::USER_NS:
			p.ns.enable_user = true;
			break;

		case SpawnExecCommand::PID_NS:
			p.ns.enable_pid = true;
			break;

		case SpawnExecCommand::PID_NS_NAME:
			p.ns.pid_namespace = payload.ReadString();
			break;

		case SpawnExecCommand::CGROUP_NS:
			p.ns.enable_cgroup = true;
			break;

		case SpawnExecCommand::NETWORK_NS:
			p.ns.enable_network = true;
			break;

		case SpawnExecCommand::NETWORK_NS_NAME:
			p.ns.network_namespace = payload.ReadString();
			break;

		case SpawnExecCommand::IPC_NS:
			p.ns.enable_ipc = true;
			break;

		case SpawnExecCommand::MOUNT_PROC:
			p.ns.mount.mount_proc = true;
			break;

		case SpawnExecCommand::WRITABLE_PROC:
			p.ns.mount.writable_proc = true;
			break;

		case SpawnExecCommand::MOUNT_DEV:
			p.ns.mount.mount_dev = true;
			break;

		case SpawnExecCommand::MOUNT_PTS:
			p.ns.mount.mount_pts = true;
			break;

		case SpawnExecCommand::BIND_MOUNT_PTS:
			p.ns.mount.bind_mount_pts = true;
			break;

		case SpawnExecCommand::PIVOT_ROOT:
			p.ns.mount.pivot_root = payload.ReadString();
			break;

		case SpawnExecCommand::MOUNT_ROOT_TMPFS:
			p.ns.mount.mount_root_tmpfs = true;
			break;

		case SpawnExecCommand::MOUNT_TMP_TMPFS:
			p.ns.mount.mount_tmp_tmpfs = payload.ReadString();
			break;

		case SpawnExecCommand::MOUNT_TMPFS:
			{
				const char *target = payload.ReadString();
				bool writable = payload.ReadBool();
				mounts.emplace_front(Mount::Tmpfs{}, target,
						     writable);
			}

			mount_tail = p.ns.mount.mounts.insert_after(mount_tail,
								    mounts.front());
			break;

		case SpawnExecCommand::BIND_MOUNT:
			{
				const char *source = payload.ReadString();
				const char *target = payload.ReadString();
				bool writable = payload.ReadBool();
				bool exec = payload.ReadBool();
				mounts.emplace_front(source, target,
						     writable, exec);
				mounts.front().optional = payload.ReadBool();
			}

			mount_tail = p.ns.mount.mounts.insert_after(mount_tail,
								    mounts.front());
			break;

		case SpawnExecCommand::BIND_MOUNT_FILE:
			{
				const char *source = payload.ReadString();
				const char *target = payload.ReadString();
				mounts.emplace_front(source, target);
				mounts.front().type = Mount::Type::BIND_FILE;
				mounts.front().optional = payload.ReadBool();
			}

			mount_tail = p.ns.mount.mounts.insert_after(mount_tail,
								    mounts.front());
			break;

		case SpawnExecCommand::WRITE_FILE:
			{
				const char *path = payload.ReadString();
				const char *contents = payload.ReadString();
				mounts.emplace_front(Mount::WriteFile{},
						     path, contents);
				mounts.front().optional = payload.ReadBool();
			}

			mount_tail = p.ns.mount.mounts.insert_after(mount_tail,
								    mounts.front());
			break;

		case SpawnExecCommand::HOSTNAME:
			p.ns.hostname = payload.ReadString();
			break;

		case SpawnExecCommand::RLIMIT:
			Read(payload, p.rlimits);
			break;

		case SpawnExecCommand::UID_GID:
			Read(payload, p.uid_gid);
			break;

		case SpawnExecCommand::SCHED_IDLE_:
			p.sched_idle = true;
			break;

		case SpawnExecCommand::IOPRIO_IDLE:
			p.ioprio_idle = true;
			break;

		case SpawnExecCommand::FORBID_USER_NS:
			p.forbid_user_ns = true;
			break;

		case SpawnExecCommand::FORBID_MULTICAST:
			p.forbid_multicast = true;
			break;

		case SpawnExecCommand::FORBID_BIND:
			p.forbid_bind = true;
			break;

		case SpawnExecCommand::NO_NEW_PRIVS:
			p.no_new_privs = true;
			break;

		case SpawnExecCommand::CGROUP:
			if (p.cgroup != nullptr)
				throw MalformedSpawnPayloadError();

			cgroup.name = payload.ReadString();
			p.cgroup = &cgroup;
			break;

		case SpawnExecCommand::CGROUP_SESSION:
			if (p.cgroup == nullptr)
				throw MalformedSpawnPayloadError();

			cgroup.session = payload.ReadString();
			break;

		case SpawnExecCommand::CGROUP_SET:
			if (p.cgroup != nullptr) {
				const char *set_name = payload.ReadString();
				const char *set_value = payload.ReadString();
				strings.emplace_front(set_name);
				set_name = strings.front().c_str();
				strings.emplace_front(set_value);
				set_value = strings.front().c_str();

				assignments.emplace_front(set_name, set_value);
				auto &set = assignments.front();
				cgroup.set.Add(set);
			} else
				throw MalformedSpawnPayloadError();

			break;

		case SpawnExecCommand::CGROUP_XATTR:
			if (p.cgroup != nullptr) {
				const char *_name = payload.ReadString();
				const char *_value = payload.ReadString();
				strings.emplace_front(_name);
				_name = strings.front().c_str();
				strings.emplace_front(_value);
				_value = strings.front().c_str();

				assignments.emplace_front(_name, _value);
				cgroup.xattr.Add(assignments.front());
			} else
				throw MalformedSpawnPayloadError();

			break;

		case SpawnExecCommand::PRIORITY:
			payload.ReadInt(p.priority);
			break;

		case SpawnExecCommand::CHROOT:
			p.chroot = payload.ReadString();
			break;

		case SpawnExecCommand::CHDIR:
			p.chdir = payload.ReadString();
			break;

		case SpawnExecCommand::HOOK_INFO:
			p.hook_info = payload.ReadString();
			break;
		}
	}

	SpawnChild(id, name, std::move(p));
}

inline void
SpawnServerConnection::HandleOneKill(SpawnPayload &payload)
{
	unsigned id;
	int signo;
	payload.ReadUnsigned(id);
	payload.ReadInt(signo);

	auto i = children.find(id);
	if (i == children.end())
		return;

	SpawnServerChild *child = &*i;
	children.erase(i);

	child->Kill(process.GetChildProcessRegistry(), signo);
	delete child;
}

inline void
SpawnServerConnection::HandleKillMessage(SpawnPayload payload,
					 SpawnFdList &&fds)
{
	if (!fds.IsEmpty())
		throw MalformedSpawnPayloadError();

	while (!payload.empty())
		HandleOneKill(payload);
}

inline void
SpawnServerConnection::HandleMessage(std::span<const std::byte> payload,
				     SpawnFdList &&fds)
{
	const auto cmd = (SpawnRequestCommand)payload.front();
	payload = payload.subspan(1);

	switch (cmd) {
	case SpawnRequestCommand::CONNECT:
		if (!payload.empty() || fds.size() != 1)
			throw MalformedSpawnPayloadError();

		process.AddConnection(fds.GetSocket());
		break;

	case SpawnRequestCommand::EXEC:
		HandleExecMessage(SpawnPayload(payload), std::move(fds));
		break;

	case SpawnRequestCommand::KILL:
		HandleKillMessage(SpawnPayload(payload), std::move(fds));
		break;
	}
}

inline void
SpawnServerConnection::HandleMessage(ReceiveMessageResult &&result)
{
	HandleMessage(result.payload, std::move(result.fds));
}

inline void
SpawnServerConnection::ReceiveAndHandle()
{
	ReceiveMessageBuffer<8192, CMSG_SPACE(sizeof(int) * 32)> rmb;

	auto result = ReceiveMessage(socket, rmb, MSG_DONTWAIT);
	if (result.payload.empty()) {
		RemoveConnection();
		return;
	}

	try {
		HandleMessage(std::move(result));
	} catch (MalformedSpawnPayloadError) {
		logger(3, "Malformed spawn payload");
	}
}

inline void
SpawnServerConnection::FlushExitQueue()
{
	if (exit_queue.empty())
		return;

	SpawnSerializer s(SpawnResponseCommand::EXIT);

	for (std::size_t n = 0; n < 64 && !exit_queue.empty(); ++n) {
		const auto &i = exit_queue.front();

		s.WriteUnsigned(i.id);
		s.WriteInt(i.status);

		exit_queue.pop_front();
	}

	::Send<1>(socket, s);
}

inline void
SpawnServerConnection::OnSocketEvent(unsigned events) noexcept
try {
	if (events & event.ERROR)
		throw MakeErrno(socket.GetError(), "Spawner socket error");

	if (events & event.HANGUP) {
		RemoveConnection();
		return;
	}

	if (events & event.WRITE) {
		FlushExitQueue();

		if (exit_queue.empty())
			event.CancelWrite();
	}

	if (events & event.READ)
		ReceiveAndHandle();
} catch (...) {
	logger(2, std::current_exception());
	RemoveConnection();
}

static void
AnnounceCgroup(SocketDescriptor s) noexcept
{
	static constexpr auto cmd = SpawnResponseCommand::CGROUPS_AVAILABLE;
	send(s.Get(), &cmd, sizeof(cmd), MSG_NOSIGNAL);
}

void
RunSpawnServer(const SpawnConfig &config, const CgroupState &cgroup_state,
	       SpawnHook *hook,
	       UniqueSocketDescriptor socket)
{
	if (cgroup_state.IsEnabled()) {
		/* tell the client that the cgroups feature is available;
		   there is no other way for the client to know if we don't
		   tell him; see SpawnServerClient::SupportsCgroups() */
		AnnounceCgroup(socket);
	}

	SpawnServerProcess process(config, cgroup_state, hook);
	process.AddConnection(std::move(socket));
	process.Run();
}
