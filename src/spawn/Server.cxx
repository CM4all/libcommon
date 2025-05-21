// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Server.hxx"
#include "Config.hxx"
#include "IProtocol.hxx"
#include "Parser.hxx"
#include "Builder.hxx"
#include "Prepared.hxx"
#include "CgroupOptions.hxx"
#include "Hook.hxx"
#include "Mount.hxx"
#include "CgroupState.hxx"
#include "Direct.hxx"
#include "Registry.hxx"
#include "TmpfsManager.hxx"
#include "ZombieReaper.hxx"
#include "ExitListener.hxx"
#include "PidfdEvent.hxx"
#include "spawn/config.h"
#include "event/CoarseTimerEvent.hxx"
#include "event/SocketEvent.hxx"
#include "event/Loop.hxx"
#include "net/EasyMessage.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "net/ReceiveMessage.hxx"
#include "net/SocketError.hxx"
#include "io/MakeDirectory.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "util/DeleteDisposer.hxx"
#include "util/IntrusiveHashSet.hxx"
#include "util/IntrusiveList.hxx"
#include "util/Exception.hxx"
#include "util/SharedLease.hxx"

#ifdef HAVE_LIBCAP
#include "lib/cap/Glue.hxx"
#endif

#include <forward_list>
#include <optional>
#include <memory>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>

class SpawnServerProcess;

using namespace Spawn;

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
			throw MalformedPayloadError();

		return std::move(*i++);
	}

	UniqueSocketDescriptor GetSocket() {
		return UniqueSocketDescriptor{Get()};
	}

	/**
	 * Like Get(), but does not transfer ownership to the caller.
	 */
	FileDescriptor Borrow() {
		if (IsEmpty())
			throw MalformedPayloadError();

		return *i++;
	}
};

class SpawnServerConnection;

class SpawnServerChild final : public ExitListener,
			       public IntrusiveHashSetHook<IntrusiveHookMode::NORMAL>
{
	SpawnServerConnection &connection;

	const std::forward_list<SharedLease> leases;

	const unsigned id;

	std::unique_ptr<PidfdEvent> pidfd;

public:
	explicit SpawnServerChild(EventLoop &event_loop,
				  SpawnServerConnection &_connection,
				  std::forward_list<SharedLease> &&_leases,
				  unsigned _id, UniqueFileDescriptor _pidfd,
				  const char *_name) noexcept
		:connection(_connection),
		 leases(std::move(_leases)),
		 id(_id),
		 pidfd(std::make_unique<PidfdEvent>(event_loop,
						    std::move(_pidfd),
						    _name,
						    (ExitListener &)*this)) {}

	SpawnServerChild(const SpawnServerChild &) = delete;
	SpawnServerChild &operator=(const SpawnServerChild &) = delete;

	void Kill(ChildProcessRegistry &child_process_registry,
		  int signo) noexcept {
		child_process_registry.Kill(std::move(pidfd), signo);
	}

	/* virtual methods from ExitListener */
	void OnChildProcessExit(int status) noexcept override;

	struct GetKey {
		unsigned operator()(const SpawnServerChild &child) const noexcept {
			return child.id;
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

	using ChildIdMap =
		IntrusiveHashSet<SpawnServerChild, 1024,
				 IntrusiveHashSetOperators<SpawnServerChild,
							   SpawnServerChild::GetKey,
							   std::hash<unsigned>,
							   std::equal_to<unsigned>>>;
	ChildIdMap children;

	struct ExecCompleteItem {
		unsigned id;
		std::string error;
	};

	std::forward_list<ExecCompleteItem> exec_complete_queue;

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

private:
	void RemoveConnection() noexcept;

	void SendExecComplete(unsigned id, std::string &&error) noexcept;
	void SendExit(unsigned id, int status) noexcept;
	void SpawnChild(unsigned id, const char *name,
			PreparedChildProcess &&p);

	void HandleExecMessage(Payload payload, SpawnFdList &&fds);
	void HandleOneKill(Payload &payload);
	void HandleKillMessage(Payload payload, SpawnFdList &&fds);
	void HandleMessage(std::span<const std::byte> payload, SpawnFdList &&fds);
	void HandleMessage(ReceiveMessageResult &&result);

	void ReceiveAndHandle();
	void FlushExecCompleteQueue();
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

static UniqueFileDescriptor
MakeTmpfsMountRoot()
{
	// TODO hard-coded path
	return MakeDirectory(FileDescriptor::Undefined(), "/tmp/tmpfs",
			     {.mode = 0100});
}

class SpawnServerProcess {
	const SpawnConfig config;
	const CgroupState &cgroup_state;
	SpawnHook *const hook;

	const LLogger logger;

	EventLoop loop;

	CoarseTimerEvent expire_timer{loop, BIND_THIS_METHOD(OnExpireTimer)};

	std::optional<TmpfsManager> tmpfs_manager;

	ChildProcessRegistry child_process_registry;

	ZombieReaper zombie_reaper{loop};

	using ConnectionList = IntrusiveList<SpawnServerConnection>;
	ConnectionList connections;

#ifdef HAVE_LIBCAP
	const bool is_sys_admin = ::IsSysAdmin();
#else
	const bool is_sys_admin = geteuid() == 0;
#endif

public:
	SpawnServerProcess(const SpawnConfig &_config,
			   const CgroupState &_cgroup_state,
			   bool has_mount_namespace,
			   SpawnHook *_hook)
		:config(_config), cgroup_state(_cgroup_state), hook(_hook),
		 logger("spawn")
	{
		if (has_mount_namespace) {
			tmpfs_manager.emplace(MakeTmpfsMountRoot());

			ScheduleExpireTimer();
		}
	}

	const SpawnConfig &GetConfig() const noexcept {
		return config;
	}

	const CgroupState &GetCgroupState() const noexcept {
		return cgroup_state;
	}

	auto &GetTmpfsManager() noexcept {
		return tmpfs_manager;
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
	void ScheduleExpireTimer() noexcept {
		expire_timer.Schedule(std::chrono::minutes{2});
	}

	void OnExpireTimer() noexcept {
		assert(tmpfs_manager);
		tmpfs_manager->Expire();

		ScheduleExpireTimer();
	}

	void Quit() noexcept {
		assert(connections.empty());

		zombie_reaper.Disable();
		expire_timer.Cancel();
	}
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

inline void
SpawnServerConnection::SendExecComplete(unsigned id, std::string &&error) noexcept
{
	if (exec_complete_queue.empty())
		event.ScheduleWrite();

	exec_complete_queue.emplace_front(id, std::move(error));
}

void
SpawnServerConnection::SendExit(unsigned id, int status) noexcept
{
	if (exit_queue.empty())
		event.ScheduleWrite();

	exit_queue.push_front({id, status});
}

/**
 * Resolve all NAMED_TMPFS using the TmpfsManager.
 */
static void
PrepareNamedTmpfs(TmpfsManager &tmpfs_manager,
		  MountNamespaceOptions &options,
		  std::forward_list<SharedLease> &leases,
		  std::forward_list<UniqueFileDescriptor> &fds)
{
	for (auto &i : options.mounts) {
		if (i.type == Mount::Type::NAMED_TMPFS &&
		    !i.source_fd.IsDefined()) {
			auto tmpfs = tmpfs_manager.MakeTmpfs(i.source, i.exec);
			fds.emplace_front(std::move(tmpfs.first));
			i.source_fd = fds.front();
			leases.emplace_front(std::move(tmpfs.second));
		}
	}
}

inline void
SpawnServerConnection::SpawnChild(unsigned id, const char *name,
				  PreparedChildProcess &&p)
{
	const auto &config = process.GetConfig();

	if (!p.uid_gid.IsEmpty()) {
		if (!process.Verify(p))
			config.Verify(p.uid_gid);
	}

	if (p.uid_gid.IsEmpty()) {
		if (config.default_uid_gid.IsEmpty())
			throw std::invalid_argument{"No uid/gid specified"};

		p.uid_gid = config.default_uid_gid;
	}

	std::forward_list<SharedLease> leases;

	std::forward_list<UniqueFileDescriptor> fds;
	if (auto &tmpfs_manager = process.GetTmpfsManager())
                PrepareNamedTmpfs(*tmpfs_manager,
                                  p.ns.mount, leases, fds);

	auto pid = SpawnChildProcess(std::move(p),
				     process.GetCgroupState(),
				     config.cgroups_writable_by_gid > 0,
				     process.IsSysAdmin()).first;

	auto *child = new SpawnServerChild(GetEventLoop(), *this,
					   std::move(leases),
					   id,
					   std::move(pid), name);
	children.insert(*child);
}

static void
Read(Payload &payload, ResourceLimits &rlimits)
{
	unsigned i = (unsigned)payload.ReadByte();
	struct rlimit &data = rlimits.values[i];
	payload.ReadT(data);
}

static void
Read(Payload &payload, UidGid &uid_gid)
{
	payload.ReadT(uid_gid.real_uid);
	payload.ReadT(uid_gid.real_gid);
	payload.ReadT(uid_gid.effective_uid);
	payload.ReadT(uid_gid.effective_gid);

	const size_t n_groups = (std::size_t)payload.ReadByte();
	if (n_groups > uid_gid.supplementary_groups.max_size())
		throw MalformedPayloadError();

	for (size_t i = 0; i < n_groups; ++i)
		payload.ReadT(uid_gid.supplementary_groups[i]);

	if (n_groups < uid_gid.supplementary_groups.max_size())
		uid_gid.supplementary_groups[n_groups] = UidGid::UNSET_GID;
}

inline void
SpawnServerConnection::HandleExecMessage(Payload payload,
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
		const ExecCommand cmd = static_cast<ExecCommand>(payload.ReadByte());
		switch (cmd) {
		case ExecCommand::EXEC_FUNCTION:
			payload.ReadT(p.exec_function);
			break;

		case ExecCommand::EXEC_PATH:
			p.exec_path = payload.ReadString();
			break;

		case ExecCommand::EXEC_FD:
			p.exec_fd = fds.Borrow();
			break;

		case ExecCommand::ARG:
			if (p.args.size() >= 16384)
				throw MalformedPayloadError();

			p.Append(payload.ReadString());
			break;

		case ExecCommand::SETENV:
			if (p.env.size() >= 16384)
				throw MalformedPayloadError();

			p.PutEnv(payload.ReadString());
			break;

		case ExecCommand::UMASK:
			{
				uint16_t value;
				payload.ReadT(value);
				p.umask = value;
			}

			break;

		case ExecCommand::STDIN:
			p.stdin_fd = fds.Borrow();
			break;

		case ExecCommand::STDOUT:
			p.stdout_fd = fds.Borrow();
			break;

		case ExecCommand::STDOUT_IS_STDIN:
			p.stdout_fd = p.stdin_fd;
			break;

		case ExecCommand::STDERR:
			p.stderr_fd = fds.Borrow();
			break;

		case ExecCommand::STDERR_IS_STDIN:
			p.stderr_fd = p.stdin_fd;
			break;

		case ExecCommand::STDERR_PATH:
			p.stderr_path = payload.ReadString();
			break;

		case ExecCommand::RETURN_STDERR:
			p.return_stderr = UniqueSocketDescriptor{fds.Get()};
			break;

		case ExecCommand::RETURN_PIDFD:
			p.return_pidfd = UniqueSocketDescriptor{fds.Get()};
			break;

		case ExecCommand::RETURN_CGROUP:
			p.return_cgroup = UniqueSocketDescriptor{fds.Get()};
			break;

		case ExecCommand::CONTROL:
			p.control_fd = fds.Borrow();
			break;

		case ExecCommand::TTY:
			p.tty = true;
			break;

		case ExecCommand::USER_NS:
			p.ns.enable_user = true;
			break;

		case ExecCommand::PID_NS:
			p.ns.enable_pid = true;
			break;

		case ExecCommand::PID_NS_NAME:
			p.ns.pid_namespace = payload.ReadString();
			break;

		case ExecCommand::CGROUP_NS:
			p.ns.enable_cgroup = true;
			break;

		case ExecCommand::NETWORK_NS:
			p.ns.enable_network = true;
			break;

		case ExecCommand::NETWORK_NS_NAME:
			p.ns.network_namespace = payload.ReadString();
			break;

		case ExecCommand::IPC_NS:
			p.ns.enable_ipc = true;
			break;

		case ExecCommand::MOUNT_PROC:
			p.ns.mount.mount_proc = true;
			break;

		case ExecCommand::WRITABLE_PROC:
			p.ns.mount.writable_proc = true;
			break;

		case ExecCommand::MOUNT_DEV:
			p.ns.mount.mount_dev = true;
			break;

		case ExecCommand::MOUNT_PTS:
			p.ns.mount.mount_pts = true;
			break;

		case ExecCommand::BIND_MOUNT_PTS:
			p.ns.mount.bind_mount_pts = true;
			break;

		case ExecCommand::PIVOT_ROOT:
			p.ns.mount.pivot_root = payload.ReadString();
			break;

		case ExecCommand::MOUNT_ROOT_TMPFS:
			p.ns.mount.mount_root_tmpfs = true;
			break;

		case ExecCommand::MOUNT_TMP_TMPFS:
			p.ns.mount.mount_tmp_tmpfs = payload.ReadString();
			break;

		case ExecCommand::MOUNT_TMPFS:
			{
				const char *target = payload.ReadString();
				bool writable = payload.ReadBool();
				mounts.emplace_front(Mount::Tmpfs{}, target,
						     writable);
			}

			mount_tail = p.ns.mount.mounts.insert_after(mount_tail,
								    mounts.front());
			break;

		case ExecCommand::MOUNT_NAMED_TMPFS:
			{
				const char *source = payload.ReadString();
				const char *target = payload.ReadString();
				bool writable = payload.ReadBool();
				mounts.emplace_front(Mount::NamedTmpfs{},
						     source, target,
						     writable);
			}

			mount_tail = p.ns.mount.mounts.insert_after(mount_tail,
								    mounts.front());
			break;

		case ExecCommand::BIND_MOUNT:
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

		case ExecCommand::BIND_MOUNT_FILE:
			{
				const char *source = payload.ReadString();
				const char *target = payload.ReadString();
				mounts.emplace_front(source, target);
				mounts.front().type = Mount::Type::BIND_FILE;
				mounts.front().exec = payload.ReadBool();
				mounts.front().optional = payload.ReadBool();
			}

			mount_tail = p.ns.mount.mounts.insert_after(mount_tail,
								    mounts.front());
			break;

		case ExecCommand::FD_BIND_MOUNT:
			{
				const char *target = payload.ReadString();
				bool writable = payload.ReadBool();
				bool exec = payload.ReadBool();
				mounts.emplace_front(nullptr, target,
						     writable, exec);
				mounts.front().source_fd = fds.Borrow();
				mounts.front().optional = payload.ReadBool();
			}

			mount_tail = p.ns.mount.mounts.insert_after(mount_tail,
								    mounts.front());
			break;

		case ExecCommand::FD_BIND_MOUNT_FILE:
			{
				const char *target = payload.ReadString();
				mounts.emplace_front(nullptr, target);
				mounts.front().type = Mount::Type::BIND_FILE;
				mounts.front().source_fd = fds.Borrow();
				mounts.front().exec = payload.ReadBool();
				mounts.front().optional = payload.ReadBool();
			}

			mount_tail = p.ns.mount.mounts.insert_after(mount_tail,
								    mounts.front());
			break;

		case ExecCommand::WRITE_FILE:
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

		case ExecCommand::SYMLINK:
			{
				const char *target = payload.ReadString();
				const char *source = payload.ReadString();
				mounts.emplace_front(source, target);
				mounts.front().type = Mount::Type::SYMLINK;
			}

			mount_tail = p.ns.mount.mounts.insert_after(mount_tail,
								    mounts.front());
			break;

		case ExecCommand::DIR_MODE:
			payload.ReadT(p.ns.mount.dir_mode);
			break;

		case ExecCommand::HOSTNAME:
			p.ns.hostname = payload.ReadString();
			break;

		case ExecCommand::RLIMIT:
			Read(payload, p.rlimits);
			break;

		case ExecCommand::UID_GID:
			Read(payload, p.uid_gid);
			break;

		case ExecCommand::MAPPED_REAL_UID:
			payload.ReadT(p.ns.mapped_real_uid);
			break;

		case ExecCommand::MAPPED_EFFECTIVE_UID:
			payload.ReadT(p.ns.mapped_effective_uid);
			break;

		case ExecCommand::SCHED_IDLE_:
			p.sched_idle = true;
			break;

		case ExecCommand::IOPRIO_IDLE:
			p.ioprio_idle = true;
			break;

#ifdef HAVE_LIBSECCOMP
		case ExecCommand::FORBID_USER_NS:
			p.forbid_user_ns = true;
			break;

		case ExecCommand::FORBID_MULTICAST:
			p.forbid_multicast = true;
			break;

		case ExecCommand::FORBID_BIND:
			p.forbid_bind = true;
			break;
#endif // HAVE_LIBSECCOMP

#ifdef HAVE_LIBCAP
		case ExecCommand::CAP_SYS_RESOURCE:
			p.cap_sys_resource = true;
			break;
#endif // HAVE_LIBCAP

		case ExecCommand::NO_NEW_PRIVS:
			p.no_new_privs = true;
			break;

		case ExecCommand::CGROUP:
			if (p.cgroup != nullptr)
				throw MalformedPayloadError();

			cgroup.name = payload.ReadString();
			p.cgroup = &cgroup;
			break;

		case ExecCommand::CGROUP_SESSION:
			if (p.cgroup == nullptr)
				throw MalformedPayloadError();

			p.cgroup_session = payload.ReadString();
			break;

		case ExecCommand::CGROUP_SET:
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
				throw MalformedPayloadError();

			break;

		case ExecCommand::CGROUP_XATTR:
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
				throw MalformedPayloadError();

			break;

		case ExecCommand::PRIORITY:
			payload.ReadInt(p.priority);
			break;

		case ExecCommand::CHROOT:
			p.chroot = payload.ReadString();
			break;

		case ExecCommand::CHDIR:
			p.chdir = payload.ReadString();
			break;

		case ExecCommand::HOOK_INFO:
			p.hook_info = payload.ReadString();
			break;
		}
	}

	try {
		SpawnChild(id, name, std::move(p));
		SendExecComplete(id, {});
	} catch (...) {
		SendExecComplete(id, GetFullMessage(std::current_exception()));
		SendExit(id, W_EXITCODE(0xff, 0));
	}
}

inline void
SpawnServerConnection::HandleOneKill(Payload &payload)
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
SpawnServerConnection::HandleKillMessage(Payload payload,
					 SpawnFdList &&fds)
{
	if (!fds.IsEmpty())
		throw MalformedPayloadError();

	while (!payload.empty())
		HandleOneKill(payload);
}

inline void
SpawnServerConnection::HandleMessage(std::span<const std::byte> payload,
				     SpawnFdList &&fds)
{
	const auto cmd = static_cast<RequestCommand>(payload.front());
	payload = payload.subspan(1);

	switch (cmd) {
	case RequestCommand::CONNECT:
		if (!payload.empty() || fds.size() != 1)
			throw MalformedPayloadError();

		process.AddConnection(fds.GetSocket());
		break;

	case RequestCommand::EXEC:
		HandleExecMessage(Payload{payload}, std::move(fds));
		break;

	case RequestCommand::KILL:
		HandleKillMessage(Payload{payload}, std::move(fds));
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
	ReceiveMessageBuffer<MAX_DATAGRAM_SIZE, CMSG_SPACE(sizeof(int) * 32)> rmb;

	auto result = ReceiveMessage(socket, rmb, MSG_DONTWAIT);
	if (result.payload.empty()) {
		RemoveConnection();
		return;
	}

	try {
		HandleMessage(std::move(result));
	} catch (MalformedPayloadError) {
		logger(3, "Malformed spawn payload");
	}
}

inline void
SpawnServerConnection::FlushExecCompleteQueue()
{
	if (exec_complete_queue.empty())
		return;

	Serializer s{ResponseCommand::EXEC_COMPLETE};

	for (std::size_t n = 0; n < 64 && !exec_complete_queue.empty(); ++n) {
		const auto &i = exec_complete_queue.front();
		s.WriteUnsigned(i.id);
		s.WriteString(i.error);
		exec_complete_queue.pop_front();
	}

	::Send<1>(socket, s);
}

inline void
SpawnServerConnection::FlushExitQueue()
{
	if (exit_queue.empty())
		return;

	Serializer s(ResponseCommand::EXIT);

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
		throw MakeSocketError(socket.GetError(), "Spawner socket error");

	if (events & event.HANGUP) {
		RemoveConnection();
		return;
	}

	if (events & event.WRITE) {
		FlushExecCompleteQueue();
		FlushExitQueue();

		if (exec_complete_queue.empty() && exit_queue.empty())
			event.CancelWrite();
	}

	if (events & event.READ)
		ReceiveAndHandle();
} catch (...) {
	logger(2, std::current_exception());
	RemoveConnection();
}

void
RunSpawnServer(const SpawnConfig &config, const CgroupState &cgroup_state,
	       bool has_mount_namespace,
	       SpawnHook *hook,
	       UniqueSocketDescriptor socket)
{
	SpawnServerProcess process(config, cgroup_state, has_mount_namespace, hook);
	process.AddConnection(std::move(socket));
	process.Run();
}
