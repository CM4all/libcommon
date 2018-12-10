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

#include "SocketConfig.hxx"
#include "UniqueSocketDescriptor.hxx"
#include "IPv4Address.hxx"
#include "ToString.hxx"
#include "system/Error.hxx"

#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

void
SocketConfig::Fixup() noexcept
{
	if (!bind_address.IsNull() && bind_address.IsV6Any() &&
	    !multicast_group.IsNull() &&
	    multicast_group.GetFamily() == AF_INET)
		bind_address = IPv4Address(bind_address.GetPort());
}

UniqueSocketDescriptor
SocketConfig::Create(int type) const
{
	assert(!bind_address.IsNull());
	assert(bind_address.IsDefined());

	const int family = bind_address.GetFamily();
	const bool is_inet = family == AF_INET || family == AF_INET6;
	const bool is_tcp = is_inet && type == SOCK_STREAM;

	UniqueSocketDescriptor fd;
	if (!fd.CreateNonBlock(family, type, 0))
		throw MakeErrno("Failed to create socket");

	const char *local_path = bind_address.GetLocalPath();
	if (local_path != nullptr)
		/* delete non-abstract socket files before reusing them */
		unlink(local_path);

	if (family == AF_LOCAL) {
		if (pass_cred)
			/* we want to receive the client's UID */
			fd.SetBoolOption(SOL_SOCKET, SO_PASSCRED, true);
	}

	if (bind_address.IsV6Any())
		fd.SetV6Only(false);

	if (!interface.empty() && !fd.SetBindToDevice(interface.c_str()))
		throw MakeErrno("Failed to set SO_BINDTODEVICE");

	/* always set SO_REUSEADDR for TCP sockets to allow quick
	   restarts */
	/* set SO_REUSEADDR if we're using multicast; this option allows
	   multiple processes to join the same group on the same port */
	if ((is_tcp || !multicast_group.IsNull()) &&
	    !fd.SetReuseAddress(true))
		throw MakeErrno("Failed to set SO_REUSEADDR");

	if (reuse_port && !fd.SetReusePort())
		throw MakeErrno("Failed to set SO_REUSEPORT");

	if (free_bind && !fd.SetFreeBind())
		throw MakeErrno("Failed to set SO_FREEBIND");

	if (mode != 0)
		/* use fchmod() on the unbound socket to limit the
		   mode, in order to avoid a race condition; later we
		   need to call chmod() on the socket path because the
		   bind() aplies the umask */
		fchmod(fd.Get(), mode);

	if (!fd.Bind(bind_address)) {
		const int e = errno;

		char buffer[256];
		const char *address_string =
			ToString(buffer, sizeof(buffer), bind_address)
			? buffer
			: "?";

		throw FormatErrno(e, "Failed to bind to %s", address_string);
	}

	if (mode != 0 && local_path != nullptr && chmod(local_path, mode) < 0)
		throw FormatErrno("Failed to chmod '%s'", local_path);

	if (!multicast_group.IsNull() &&
	    !fd.AddMembership(multicast_group)) {
		const int e = errno;

		char buffer[256];
		const char *address_string =
			ToString(buffer, sizeof(buffer), multicast_group)
			? buffer
			: "?";

		throw FormatErrno(e, "Failed to join multicast group %s",
				  address_string);
	}

	if (is_tcp) {
		fd.SetTcpFastOpen();

		if (tcp_defer_accept > 0)
			fd.SetTcpDeferAccept(tcp_defer_accept);

		if (tcp_user_timeout > 0)
			fd.SetTcpUserTimeout(tcp_user_timeout);
	}

	if (keepalive)
		fd.SetKeepAlive();

	if (listen > 0 && !fd.Listen(listen))
		throw MakeErrno("Failed to listen");

	return fd;
}
