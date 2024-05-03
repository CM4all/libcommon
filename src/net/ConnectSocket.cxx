// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "ConnectSocket.hxx"
#include "UniqueSocketDescriptor.hxx"
#include "SocketAddress.hxx"
#include "SocketError.hxx"
#include "lib/fmt/SocketError.hxx"
#include "lib/fmt/SocketAddressFormatter.hxx"

UniqueSocketDescriptor
CreateConnectSocket(const SocketAddress address, int type)
{
	UniqueSocketDescriptor fd;
	if (!fd.Create(address.GetFamily(), type, 0))
		throw MakeSocketError("Failed to create socket");

	if (!fd.Connect(address))
		throw FmtSocketError("Failed to connect to {}", address);

	return fd;
}

UniqueSocketDescriptor
CreateConnectSocketNonBlock(const SocketAddress address, int type)
{
	UniqueSocketDescriptor fd;
	if (!fd.CreateNonBlock(address.GetFamily(), type, 0))
		throw MakeSocketError("Failed to create socket");

	if (!fd.Connect(address))
		throw FmtSocketError("Failed to connect to {}", address);

	return fd;
}

UniqueSocketDescriptor
CreateConnectDatagramSocket(const SocketAddress address)
{
	return CreateConnectSocketNonBlock(address, SOCK_DGRAM);
}
