// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Client.hxx"
#include "Builder.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "lib/fmt/SystemError.hxx"
#include "net/ConnectSocket.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "net/LocalSocketAddress.hxx"
#include "net/ReceiveMessage.hxx"
#include "net/SendMessage.hxx"
#include "util/CRC32.hxx"
#include "util/SpanCast.hxx"

#include <sched.h>

using std::string_view_literals::operator""sv;

namespace SpawnAccessory {

/**
 * Connect to the local Spawn daemon.
 */
UniqueSocketDescriptor
Connect()
{
	static constexpr LocalSocketAddress address{"@cm4all-spawn"sv};
	return CreateConnectSocket(address, SOCK_SEQPACKET);
}

static void
SendNamespacesRequest(SocketDescriptor s, std::string_view name,
		      const NamespacesRequest request)
{
	DatagramBuilder b;

	const RequestHeader name_header{uint16_t(name.size()), RequestCommand::NAME};
	b.Append(name_header);
	b.AppendPadded(AsBytes(name));

	if (request.ipc) {
		static constexpr RequestHeader ipc_namespace_header{0, RequestCommand::IPC_NAMESPACE};
		b.Append(ipc_namespace_header);
	}

	if (request.pid) {
		static constexpr RequestHeader pid_namespace_header{0, RequestCommand::PID_NAMESPACE};
		b.Append(pid_namespace_header);
	}

	RequestHeader user_namespace_header;
	if (request.uid_map.data() != nullptr || request.gid_map.data() != nullptr) {
		static constexpr std::byte zero{};
		const std::size_t payload_size = request.uid_map.size() + sizeof(zero) + request.gid_map.size();

		user_namespace_header = {
			static_cast<uint16_t>(payload_size),
			RequestCommand::USER_NAMESPACE,
		};
		b.Append(user_namespace_header);

		b.AppendRaw(AsBytes(request.uid_map));
		b.AppendRaw(ReferenceAsBytes(zero));
		b.AppendRaw(AsBytes(request.gid_map));
		b.Pad(payload_size);
	}

	if (request.lease_pipe) {
		static constexpr RequestHeader lease_pipe_header{0, RequestCommand::LEASE_PIPE};
		b.Append(lease_pipe_header);
	}

	SendMessage(s, b.Finish(), 0);
}

template<size_t PAYLOAD_SIZE, size_t CMSG_SIZE>
static std::pair<std::span<const std::byte>, std::vector<UniqueFileDescriptor>>
ReceiveDatagram(SocketDescriptor s,
		ReceiveMessageBuffer<PAYLOAD_SIZE, CMSG_SIZE> &buffer)
{
	auto response = ReceiveMessage(s, buffer, 0);
	auto payload = response.payload;
	const auto &dh = *(const DatagramHeader *)(const void *)payload.data();
	if (payload.size() < sizeof(dh))
		throw std::runtime_error("Response datagram too small");

	if (dh.magic != MAGIC)
		throw std::runtime_error("Wrong magic in response datagram");

	payload = payload.subspan(sizeof(dh));

	if (dh.crc != CRC32(payload))
		throw std::runtime_error("Bad CRC in response datagram");

	return {payload, std::move(response.fds)};
}

static void
ParseNamespaceHandles(NamespacesResponse &response,
		      const std::span<const std::byte> raw_payload,
		      std::span<UniqueFileDescriptor> &fds)
{
	if (raw_payload.size() % sizeof(uint32_t) != 0)
		throw std::runtime_error{"Odd NAMESPACE_HANDLES payload"};

	const auto payload = FromBytesStrict<const uint32_t>(raw_payload);
	if (fds.size() < payload.size())
		throw std::runtime_error{"Not enough file descriptors in NAMESPACE_HANDLES response"};

	for (std::size_t i = 0; i < payload.size(); ++i) {
		switch (payload[i]) {
		case CLONE_NEWIPC:
			response.ipc = std::move(fds[i]);
			break;

		case CLONE_NEWPID:
			response.pid = std::move(fds[i]);
			break;

		case CLONE_NEWUSER:
			response.user = std::move(fds[i]);
			break;

		default:
			throw std::runtime_error{"Unsupported namespace in NAMESPACE_HANDLES response"};
		}
	}

	fds = fds.subspan(payload.size());
}

static void
ParseLeasePipe(NamespacesResponse &response,
	       const std::span<const std::byte> raw_payload,
	       std::span<UniqueFileDescriptor> &fds)
{
	if (!raw_payload.empty())
		throw std::runtime_error{"Bad LEASE_PIPE payload"};

	if (fds.empty())
		throw std::runtime_error{"LEASE_PIPE without file descriptor"};

	response.lease_pipe = std::move(fds.front());
	fds = fds.subspan(1);
}

NamespacesResponse
MakeNamespaces(SocketDescriptor s, std::string_view name,
	       NamespacesRequest request)
{
	SendNamespacesRequest(s, name, request);

	ReceiveMessageBuffer<1024, CMSG_SPACE(sizeof(int) * 4)> buffer;
	auto d = ReceiveDatagram(s, buffer);
	auto payload = d.first;
	std::span<UniqueFileDescriptor> fds = d.second;

	NamespacesResponse response;

	while (!payload.empty()) {
		const auto &rh = *(const ResponseHeader *)(const void *)payload.data();
		if (payload.size() < sizeof(rh))
			throw std::runtime_error("Response datagram too small");

		payload = payload.subspan(sizeof(rh));

		if (payload.size() < rh.size)
			throw std::runtime_error("Response datagram too small");

		const size_t padded_size = (rh.size + 3) & (~3u);

		switch (rh.command) {
		case ResponseCommand::ERROR:
			throw FmtRuntimeError("Spawn server error: {}",
					      ToStringView(payload));

		case ResponseCommand::NAMESPACE_HANDLES:
			ParseNamespaceHandles(response, payload.first(rh.size), fds);
			break;

		case ResponseCommand::LEASE_PIPE:
			if (!request.lease_pipe)
				throw std::runtime_error("Unexpected LEASE_PIPE response");

			ParseLeasePipe(response, payload.first(rh.size), fds);
			break;
		}

		payload = payload.subspan(padded_size);
	}

	if (!fds.empty())
		throw std::runtime_error{"Too many file descriptors"};

	if (request.ipc && !response.ipc.IsDefined())
		throw std::runtime_error{"IPC namespace missing in response"};

	if (request.pid && !response.pid.IsDefined())
		throw std::runtime_error{"PID namespace missing in response"};

	const bool user_requested = request.uid_map.data() != nullptr || request.gid_map.data() != nullptr;
	if (user_requested && !response.user.IsDefined())
		throw std::runtime_error{"User namespace missing in response"};

	if (request.lease_pipe && !response.lease_pipe.IsDefined())
		throw std::runtime_error{"LEASE_PIPE missing in response"};

	return response;
}

}
