// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "SocketConfig.hxx"
#include "UniqueSocketDescriptor.hxx"
#include "IPv4Address.hxx"
#include "SocketError.hxx"
#include "lib/fmt/SystemError.hxx"
#include "lib/fmt/SocketError.hxx"
#include "lib/fmt/SocketAddressFormatter.hxx"

#include <cassert>

#include <sys/stat.h>
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
	const bool is_tcp = bind_address.IsInet() && type == SOCK_STREAM;

	int protocol = 0;

#ifdef IPPROTO_MPTCP
	if (is_tcp && mptcp)
		protocol = IPPROTO_MPTCP;
#endif

	UniqueSocketDescriptor fd;
	if (!fd.CreateNonBlock(family, type, protocol))
		throw MakeSocketError("Failed to create socket");

	const char *local_path = bind_address.GetLocalPath();
	if (local_path != nullptr)
		/* delete non-abstract socket files before reusing them */
		unlink(local_path);

	if (family == AF_LOCAL) {
		if (pass_cred)
			/* we want to receive the client's UID */
			fd.SetBoolOption(SOL_SOCKET, SO_PASSCRED, true);
	}

	if (v6only)
		fd.SetV6Only(true);
	else if (bind_address.IsV6Any())
		fd.SetV6Only(false);

	if (!interface.empty() && !fd.SetBindToDevice(interface.c_str()))
		throw MakeSocketError("Failed to set SO_BINDTODEVICE");

	/* always set SO_REUSEADDR for TCP sockets to allow quick
	   restarts */
	/* set SO_REUSEADDR if we're using multicast; this option allows
	   multiple processes to join the same group on the same port */
	if ((is_tcp || !multicast_group.IsNull()) &&
	    !fd.SetReuseAddress(true))
		throw MakeSocketError("Failed to set SO_REUSEADDR");

	if (reuse_port && !fd.SetReusePort())
		throw MakeSocketError("Failed to set SO_REUSEPORT");

	if (free_bind && !fd.SetFreeBind())
		throw MakeSocketError("Failed to set SO_FREEBIND");

	if (mode != 0)
		/* use fchmod() on the unbound socket to limit the
		   mode, in order to avoid a race condition; later we
		   need to call chmod() on the socket path because the
		   bind() applies the umask */
		fchmod(fd.Get(), mode);

	if (!fd.Bind(bind_address))
		throw FmtSocketError("Failed to bind to {}", bind_address);

	if (mode != 0 && local_path != nullptr && chmod(local_path, mode) < 0)
		throw FmtErrno("Failed to chmod '{}'", local_path);

	if (!multicast_group.IsNull() &&
	    !fd.AddMembership(multicast_group))
		throw FmtSocketError("Failed to join multicast group {}",
				     multicast_group);

	if (is_tcp) {
		fd.SetTcpFastOpen();

		if (tcp_defer_accept > 0)
			fd.SetTcpDeferAccept(tcp_defer_accept);

		if (tcp_user_timeout > 0)
			fd.SetTcpUserTimeout(tcp_user_timeout);

		if (tcp_no_delay)
			fd.SetNoDelay();
	}

	if (keepalive)
		fd.SetKeepAlive();

	if (listen > 0 && !fd.Listen(listen))
		throw MakeSocketError("Failed to listen");

	return fd;
}
