// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "spawn/Direct.hxx"
#include "spawn/Prepared.hxx"
#include "spawn/CgroupState.hxx"
#include "spawn/CgroupOptions.hxx"
#include "spawn/Mount.hxx"
#include "spawn/Systemd.hxx"
#include "system/Error.hxx"
#include "util/PrintException.hxx"
#include "util/StringCompare.hxx"
#include "util/StringSplit.hxx"
#include "AllocatorPtr.hxx"

#include <forward_list>
#include <span>
#include <string>

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

#if defined(__GLIBC__) && __GLIBC__+0 == 2 && __GLIBC_MINOR__+0 < 36
#include <linux/wait.h> // for P_PIDFD on glibc < 2.36
#endif

struct Usage {};

int
main(int argc, char **argv)
try {
	std::span<const char *const> args{argv + 1, static_cast<std::size_t>(argc - 1)};

	const char *scope_name = nullptr;

	Allocator alloc;
	PreparedChildProcess p;
	CgroupOptions cgroup_options;

	std::forward_list<Mount> mounts;
	std::forward_list<std::string> strings;

	auto mount_tail = p.ns.mount.mounts.before_begin();

	p.stdin_fd = FileDescriptor{STDIN_FILENO};
	p.stdout_fd = FileDescriptor{STDOUT_FILENO};
	p.stderr_fd = FileDescriptor{STDERR_FILENO};

	while (!args.empty() && *args.front() == '-') {
		const char *arg = args.front();
		args = args.subspan(1);
		if (const char *uid = StringAfterPrefix(arg, "--uid=")) {
			p.uid_gid.effective_uid = atoi(uid);
		} else if (const char *gid = StringAfterPrefix(arg, "--gid=")) {
			p.uid_gid.effective_gid = atoi(gid);
		} else if (const char *real_uid = StringAfterPrefix(arg, "--real-uid=")) {
			p.uid_gid.real_uid = atoi(real_uid);
		} else if (const char *real_gid = StringAfterPrefix(arg, "--real-gid=")) {
			p.uid_gid.real_gid = atoi(real_gid);
		} else if (const char *mapped_real_uid = StringAfterPrefix(arg, "--mapped-real-uid=")) {
			p.ns.mapped_real_uid = atoi(mapped_real_uid);
		} else if (const char *mapped_effective_uid = StringAfterPrefix(arg, "--mapped-effective-uid=")) {
			p.ns.mapped_effective_uid = atoi(mapped_effective_uid);
		} else if (StringIsEqual(arg, "--userns")) {
			p.ns.enable_user = true;
		} else if (StringIsEqual(arg, "--pidns")) {
			p.ns.enable_pid = true;
		} else if (const char *pidns = StringAfterPrefix(arg, "--pidns=")) {
			p.ns.pid_namespace = pidns;
		} else if (StringIsEqual(arg, "--netns")) {
			p.ns.enable_network = true;
		} else if (const char *netns = StringAfterPrefix(arg, "--netns=")) {
			p.ns.network_namespace = netns;
		} else if (StringIsEqual(arg, "--root-tmpfs")) {
			p.ns.mount.mount_root_tmpfs = true;
		} else if (const char *pivot_root = StringAfterPrefix(arg, "--root=")) {
			p.ns.mount.pivot_root = pivot_root;
		} else if (StringIsEqual(arg, "--mount-proc")) {
			p.ns.mount.mount_proc = true;
		} else if (StringIsEqual(arg, "--mount-pts")) {
			p.ns.mount.mount_pts = true;
		} else if (StringIsEqual(arg, "--bind-mount-pts")) {
			p.ns.mount.bind_mount_pts = true;
		} else if (const char *bind_mount = StringAfterPrefix(arg, "--bind-mount=")) {
			auto s = Split(std::string_view{bind_mount}, '=');
			if (s.first.empty() || s.second.empty())
				throw "Malformed --bind-mount parameter";

			strings.emplace_front(s.first);
			const char *source = strings.front().c_str();

			strings.emplace_front(s.second);
			const char *target = strings.front().c_str();

			mounts.emplace_front(source, target, false, false);
			mount_tail = p.ns.mount.mounts.insert_after(mount_tail,
								    mounts.front());

		} else if (const char *mount_tmpfs = StringAfterPrefix(arg, "--mount-tmpfs=")) {
			mounts.emplace_front(Mount::Tmpfs{}, mount_tmpfs, true);
			mount_tail = p.ns.mount.mounts.insert_after(mount_tail,
								    mounts.front());
		} else if (const char *mount_named_tmpfs = StringAfterPrefix(arg, "--mount-named-tmpfs=")) {
			auto s = Split(std::string_view{mount_named_tmpfs}, '=');
			if (s.first.empty() || s.second.empty())
				throw "Malformed --mount-named-tmpfs parameter";

			strings.emplace_front(s.first);
			const char *name = strings.front().c_str();

			strings.emplace_front(s.second);
			const char *target = strings.front().c_str();

			mounts.emplace_front(Mount::NamedTmpfs{},
					     name, target, true);
			mount_tail = p.ns.mount.mounts.insert_after(mount_tail,
								    mounts.front());
		} else if (const char *scope = StringAfterPrefix(arg, "--scope=")) {
			scope_name = scope;
		} else if (const char *cgroup = StringAfterPrefix(arg, "--cgroup=")) {
			if (scope_name == nullptr)
				throw "--cgroup requires --scope";

			cgroup_options.name = cgroup;
			p.cgroup = &cgroup_options;
		} else if (const char *session = StringAfterPrefix(arg, "--cgroup-session=")) {
			if (p.cgroup == nullptr)
				throw "--cgroup-session requires --cgroup";

			p.cgroup_session = session;
		} else if (const char *cgroup_set = StringAfterPrefix(arg, "--cgroup-set=")) {
			if (p.cgroup == nullptr)
				throw "--cgroup-set requires --cgroup";

			const auto [name, value] = Split(std::string_view{cgroup_set}, '=');
			if (name.empty() || value.empty())
				throw "Malformed --cgroup-set value";

			cgroup_options.Set(alloc, name, value);
		} else
			throw Usage();
	}

	if (args.empty()) {
		p.exec_path = "/bin/bash";
		p.args.emplace_back("bash");
	} else {
		p.exec_path = args.front();
		p.args.emplace_back(args.front());
		args = args.subspan(1);

		while (!args.empty()) {
			p.args.emplace_back(args.front());
			args = args.subspan(1);
		}
	}

	const CgroupState cgroup_state = scope_name != nullptr
		? CreateSystemdScope(scope_name, scope_name,
				     {},
				     getpid(), getpid(),
				     true, nullptr)
		: CgroupState();

	const auto pid = SpawnChildProcess(std::move(p), cgroup_state,
					   false,
					   geteuid() == 0).first;

	siginfo_t info;

	if (waitid((idtype_t)P_PIDFD, pid.Get(),
		   &info, WEXITED) < 0)
		throw MakeErrno("waitid() failed");

	switch (info.si_code) {
	case CLD_EXITED:
		return info.si_status;

	case CLD_KILLED:
		fprintf(stderr, "Child process died from signal %d\n",
			WTERMSIG(info.si_status));
		return EXIT_FAILURE;

	case CLD_DUMPED:
		fprintf(stderr, "Child process died from signal %d (core dumped)\n",
			WTERMSIG(info.si_status));
		return EXIT_FAILURE;

	case CLD_STOPPED:
	case CLD_TRAPPED:
	case CLD_CONTINUED:
		break;
	}

	fprintf(stderr, "Unknown child status\n");
	return EXIT_FAILURE;
} catch (Usage) {
	fprintf(stderr, "Usage: RunSpawn"
		" [--uid=#] [--gid=#] [--userns]"
		" [--pidns[=NAME]] [--netns[=NAME]]"
		" [--root-tmpfs] [--root=PATH] [--mount-proc]"
		" [--mount-pts] [--bind-mount-pts]"
		" [--bind-mount=SOURCE=TARGET]"
		" [--mount-tmpfs=TARGET]"
		" [--mount-named-tmpfs=NAME=TARGET]"
		" [--scope=NAME] [--cgroup=NAME] [--cgroup-session=ID] [--cgroup-set=NAME=VALUE]"
		" [PROGRAM ARGS...]\n");
	return EXIT_FAILURE;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
