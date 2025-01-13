// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "net/AddressInfo.hxx"
#include "net/ConnectSocket.hxx"
#include "net/Resolver.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "event/net/BufferedSocket.hxx"
#include "event/uring/Manager.hxx"
#include "event/Loop.hxx"
#include "system/Error.hxx"
#include "util/PrintException.hxx"

#include <cstdint>

#include <stdlib.h>

class NetCat final : public BufferedSocketHandler {
	Uring::Manager &uring_manager;

	BufferedSocket socket;

	std::exception_ptr error;

public:
	NetCat(EventLoop &event_loop, Uring::Manager &_uring_manager,
	       SocketDescriptor _socket)
		:uring_manager(_uring_manager), socket(event_loop)
	{
		socket.Init(_socket, FdType::FD_TCP, std::chrono::minutes{1}, *this);
		socket.EnableUring(uring_manager);
	}

	void Finish() {
		if (error)
			std::rethrow_exception(error);
	}

private:
	/* virtual methods from class BufferedSocketHandler */
	BufferedResult OnBufferedData() override {
		const auto r = socket.ReadBuffer();
		(void)FileDescriptor{STDOUT_FILENO}.Write(r);
		socket.DisposeConsumed(r.size());
		return BufferedResult::OK;
	}

	bool OnBufferedClosed() noexcept override {
		socket.Abandon();
		return true;
	}

	bool OnBufferedEnd() {
		uring_manager.SetVolatile();
		return true;
	}

	bool OnBufferedWrite() override {
		return true;
	}

	void OnBufferedError(std::exception_ptr e) noexcept override {
		uring_manager.SetVolatile();
		error = std::move(e);
	}
};

int
main(int argc, char **argv) noexcept
try {
	if (argc != 2)
		throw "Usage: UringNetCat HOST:PORT";

	static constexpr auto hints = MakeAddrInfo(AI_ADDRCONFIG, AF_UNSPEC,
						   SOCK_STREAM);

	const auto socket = CreateConnectSocket(Resolve(argv[1], 80, &hints).GetBest(), SOCK_STREAM);

	EventLoop event_loop;
	Uring::Manager uring_queue{event_loop, 64};

	NetCat net_cat{event_loop, uring_queue, socket};

	event_loop.Run();

	net_cat.Finish();

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
