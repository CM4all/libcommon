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

#include "Glue.hxx"
#include "Client.hxx"
#include "Launch.hxx"
#include "Registry.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "system/Error.hxx"

#include <sys/socket.h>

std::unique_ptr<SpawnServerClient>
StartSpawnServer(const SpawnConfig &config,
		 ChildProcessRegistry &child_process_registry,
		 SpawnHook *hook,
		 std::function<void()> post_clone)
{
	UniqueSocketDescriptor s1, s2;
	if (!UniqueSocketDescriptor::CreateSocketPairNonBlock(AF_LOCAL, SOCK_SEQPACKET, 0,
							      s1, s2))
		throw MakeErrno("socketpair() failed");

	auto pid = LaunchSpawnServer(config, hook, std::move(s1),
				     [&s2, post_clone](){
					     s2.Close();
					     post_clone();
				     });

	child_process_registry.Add(pid, "spawn", nullptr);

	return std::make_unique<SpawnServerClient>
		(child_process_registry.GetEventLoop(),
		 config, std::move(s2),
		 /* don't verify if there is a hook, because the hook
		    may have its own overriding rules */
		 hook == nullptr);
}
