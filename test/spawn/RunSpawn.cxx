/*
 * Copyright 2007-2022 CM4all GmbH
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

#include "spawn/Direct.hxx"
#include "spawn/Prepared.hxx"
#include "spawn/CgroupState.hxx"
#include "spawn/CgroupOptions.hxx"
#include "spawn/Mount.hxx"
#include "spawn/Systemd.hxx"
#include "system/Error.hxx"
#include "util/ConstBuffer.hxx"
#include "util/PrintException.hxx"
#include "util/StringCompare.hxx"
#include "util/StringSplit.hxx"
#include "AllocatorPtr.hxx"

#include <forward_list>
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
	ConstBuffer<const char *> args(argv + 1, argc - 1);

	const char *scope_name = nullptr;

	Allocator alloc;
	PreparedChildProcess p;
	CgroupOptions cgroup_options;

	std::forward_list<Mount> mounts;
	std::forward_list<std::string> strings;

	auto mount_tail = p.ns.mount.mounts.before_begin();

	p.exec_path = "/bin/bash";
	p.args.emplace_back("bash");
	p.stdin_fd = FileDescriptor{dup(STDIN_FILENO)};
	p.stdout_fd = FileDescriptor{dup(STDOUT_FILENO)};
	p.stderr_fd = FileDescriptor{dup(STDERR_FILENO)};

	while (!args.empty() && *args.front() == '-') {
		const char *arg = args.shift();
		if (const char *uid = StringAfterPrefix(arg, "--uid=")) {
			p.uid_gid.uid = atoi(uid);
		} else if (const char *gid = StringAfterPrefix(arg, "--gid=")) {
			p.uid_gid.gid = atoi(gid);
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

			cgroup_options.session = session;
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

	const CgroupState cgroup_state = scope_name != nullptr
		? CreateSystemdScope(scope_name, scope_name,
				     {},
				     getpid(), true, nullptr)
		: CgroupState();

	const auto pid = SpawnChildProcess(std::move(p), cgroup_state,
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
		" [--scope=NAME] [--cgroup=NAME] [--cgroup-session=ID] [--cgroup-set=NAME=VALUE]"
		"\n");
	return EXIT_FAILURE;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
