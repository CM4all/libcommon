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
#include <sys/un.h>
#include <unistd.h>

void
SocketConfig::Fixup()
{
	if (bind_address.IsV6Any() && multicast_group.IsDefined() &&
	    multicast_group.GetFamily() == AF_INET)
		bind_address = IPv4Address(bind_address.GetPort());
}

UniqueSocketDescriptor
SocketConfig::Create(int type) const
{
	assert(!bind_address.IsNull());
	assert(bind_address.IsDefined());

	const int family = bind_address.GetFamily();

	UniqueSocketDescriptor fd;
	if (!fd.CreateNonBlock(family, type, 0))
		throw MakeErrno("Failed to create socket");

	if (family == AF_LOCAL) {
		const struct sockaddr_un *sun = (const struct sockaddr_un *)
			bind_address.GetAddress();
		if (sun->sun_path[0] != '\0')
			/* delete non-abstract socket files before reusing them */
			unlink(sun->sun_path);

		if (pass_cred)
			/* we want to receive the client's UID */
			fd.SetBoolOption(SOL_SOCKET, SO_PASSCRED, true);
	}

	if (bind_address.IsV6Any())
		fd.SetV6Only(false);

	if (!interface.empty() && !fd.SetBindToDevice(interface.c_str()))
		throw MakeErrno("Failed to set SO_BINDTODEVICE");

	/* set SO_REUSEADDR if we're using multicast; this option allows
	   multiple processes to join the same group on the same port */
	if (!multicast_group.IsNull() && !fd.SetReuseAddress(true))
		throw MakeErrno("Failed to set SO_REUSEADDR");

	if (!fd.Bind(bind_address)) {
		const int e = errno;

		char buffer[256];
		const char *address_string =
			ToString(buffer, sizeof(buffer), bind_address)
			? buffer
			: "?";

		throw FormatErrno(e, "Failed to bind to %s", address_string);
	}

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

	return fd;
}
