/*
 * Copyright 2018 Content Management AG
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
#include "spawn/Systemd.hxx"
#include "system/Error.hxx"
#include "util/PrintException.hxx"
#include "util/StringCompare.hxx"
#include "AllocatorPtr.hxx"

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

struct Usage {};

int
main(int argc, char **argv)
try {
	ConstBuffer<const char *> args(argv + 1, argc - 1);

	const char *scope_name;

	Allocator alloc;
	PreparedChildProcess p;
	CgroupOptions cgroup_options;

	p.exec_path = "/bin/bash";
	p.args.emplace_back("bash");
	p.stdin_fd = dup(STDIN_FILENO);
	p.stdout_fd = dup(STDOUT_FILENO);
	p.stderr_fd = dup(STDERR_FILENO);
	p.session = false;

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
		} else if (const char *pivot_root = StringAfterPrefix(arg, "--root=")) {
			p.ns.mount.enable_mount = true;
			p.ns.mount.pivot_root = pivot_root;
		} else if (StringIsEqual(arg, "--mount-proc")) {
			p.ns.mount.enable_mount = true;
			p.ns.mount.mount_proc = true;
		} else if (const char *scope = StringAfterPrefix(arg, "--scope=")) {
			scope_name = scope;
		} else if (const char *cgroup = StringAfterPrefix(arg, "--cgroup=")) {
			if (scope_name == nullptr)
				throw "--cgroup requires --scope";

			cgroup_options.name = cgroup;
			p.cgroup = &cgroup_options;
		} else if (const char *cgroup_set = StringAfterPrefix(arg, "--cgroup-set=")) {
			if (p.cgroup == nullptr)
				throw "--cgroup-set requires --cgroup";

			const char *eq = strchr(cgroup_set, '=');
			if (eq == nullptr || eq == cgroup_set)
				throw "Malformed --cgroup-set value";

			cgroup_options.Set(alloc, StringView(cgroup_set, eq),
					   StringView(eq + 1));
		} else
			throw Usage();
	}

	const CgroupState cgroup_state = scope_name != nullptr
		? CreateSystemdScope(scope_name, scope_name,
				     getpid(), true, nullptr)
		: CgroupState();

	const auto pid = SpawnChildProcess(std::move(p), cgroup_state);

	int status;
	if (waitpid(pid, &status, 0) < 0)
		throw MakeErrno("waitpid() failed");

	if (WIFSIGNALED(status)) {
		fprintf(stderr, "Child process died from signal %d%s\n",
			WTERMSIG(status),
			WCOREDUMP(status) ? " (core dumped)" : "");
		return EXIT_FAILURE;
	}

	if (WIFEXITED(status))
		return WEXITSTATUS(status);

	fprintf(stderr, "Unknown child status\n");
	return EXIT_FAILURE;
} catch (const char *msg) {
	fprintf(stderr, "%s\n", msg);
	return EXIT_FAILURE;
} catch (Usage) {
	fprintf(stderr, "Usage: RunSpawn"
		" [--uid=#] [--gid=#] [--userns]"
		" [--pidns[=NAME]] [--netns[=NAME]]"
		" [--root=PATH] [--mount-proc]"
		" [--scope=NAME] [--cgroup=NAME] [--cgroup-set=NAME=VALUE]"
		"\n");
	return EXIT_FAILURE;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
