// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "AllocatorPtr.hxx"
#include "translation/Protocol.hxx"
#include "translation/PReader.hxx"
#include "translation/String.hxx"
#include "net/ConnectSocket.hxx"
#include "net/LocalSocketAddress.hxx"
#include "net/SocketError.hxx"
#include "net/SocketProtocolError.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "util/SpanCast.hxx"
#include "util/StringSplit.hxx"
#include "util/PrintException.hxx"

#include <fmt/core.h>

#include <sysexits.h> // for EX_*

using std::string_view_literals::operator""sv;

static void
AppendPacket(std::string &request, const TranslationHeader &header, std::string_view payload) noexcept
{
	request += ToStringView(ReferenceAsBytes(header));
	request += payload;
}

static std::string
ParseRequest(std::span<const char * const> args)
{
	std::string request;

	// Add implicit BEGIN packet
	static constexpr TranslationHeader begin_header{
		.length = 0,
		.command = TranslationCommand::BEGIN,
	};
	AppendPacket(request, begin_header, {});

	// Parse and add command line packets
	for (const char *arg : args) {
		std::string_view arg_view = arg;
		auto [cmd_str, payload] = Split(arg_view, '=');
		if (payload.data() == nullptr)
			throw std::runtime_error(fmt::format("Invalid packet format (expected COMMAND=PAYLOAD): {:?}", arg));

		TranslationCommand command = ParseTranslationCommand(cmd_str);

		TranslationHeader header{
			.length = static_cast<uint16_t>(payload.size()),
			.command = command,
		};
		AppendPacket(request, header, payload);
	}

	// Add END packet
	static constexpr TranslationHeader end_header{
		.length = 0,
		.command = TranslationCommand::END,
	};
	AppendPacket(request, end_header, {});

	return request;
}

static void
DumpPacket(TranslationCommand command, std::span<const std::byte> payload)
{
	if (payload.empty())
		fmt::print("{}\n", ToString(command));
	else
		fmt::print("{} = {:?}\n", ToString(command), ToStringView(payload));
}

static void
ReadAndProcessResponse(SocketDescriptor socket, AllocatorPtr alloc)
{
	TranslatePacketReader reader;

	while (true) {
		std::array<std::byte, 4096> buffer;
		ssize_t bytes_read = socket.Read(buffer);
		if (bytes_read <= 0) [[unlikely]] {
			if (bytes_read < 0)
				throw MakeSocketError("Failed to read from socket");
			else
				throw SocketClosedPrematurelyError{};
		}

		auto received = std::span{buffer}.first(bytes_read);
		while (true) {
			const auto consumed = reader.Feed(alloc, received);
			if (consumed == 0)
				// Need more data
				break;

			received = received.subspan(consumed);

			if (reader.IsComplete()) {
				const auto command = reader.GetCommand();
				DumpPacket(command, reader.GetPayload());

				if (command == TranslationCommand::END)
					return;
			}
		}
	}
}


int
main(int argc, char **argv)
try {
	if (argc < 2) {
		fmt::print(stderr,
			   "Usage: {} SOCKET_PATH [COMMAND=PAYLOAD] ...\nExample: {} /tmp/translation.sock HOST=example.com URI=/path"sv,
			   argv[0], argv[0]);
		return EX_USAGE;
	}

	const char *const socket_path = argv[1];
	const auto request = ParseRequest({argv + 2, static_cast<size_t>(argc - 2)});

	auto socket = CreateConnectSocket(LocalSocketAddress{socket_path}, SOCK_STREAM);
	socket.FullWrite(AsBytes(request));

	Allocator allocator_instance;
	AllocatorPtr alloc{allocator_instance};
	ReadAndProcessResponse(socket, alloc);

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
