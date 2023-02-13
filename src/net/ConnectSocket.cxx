// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "ConnectSocket.hxx"
#include "UniqueSocketDescriptor.hxx"
#include "SocketAddress.hxx"
#include "SocketError.hxx"
#include "ToString.hxx"

UniqueSocketDescriptor
CreateConnectSocket(const SocketAddress address, int type)
{
	UniqueSocketDescriptor fd;
	if (!fd.CreateNonBlock(address.GetFamily(), type, 0))
		throw MakeSocketError("Failed to create socket");

	if (!fd.Connect(address)) {
		const auto code = GetSocketError();
		char buffer[256];
		ToString(buffer, sizeof(buffer), address);
		throw FormatSocketError(code, "Failed to connect to %s", buffer);
	}

	return fd;
}

UniqueSocketDescriptor
CreateConnectDatagramSocket(const SocketAddress address)
{
	return CreateConnectSocket(address, SOCK_DGRAM);
}
