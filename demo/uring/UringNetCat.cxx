// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "net/AddressInfo.hxx"
#include "net/Resolver.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "event/net/BufferedSocket.hxx"
#include "event/net/ConnectSocket.hxx"
#include "io/uring/Queue.hxx"
#include "event/Loop.hxx"
#include "system/Error.hxx"
#include "util/PrintException.hxx"

#include <cstdint>

#include <stdlib.h>

class NetCat final : ConnectSocketHandler, BufferedSocketHandler {
	ConnectSocket connect_socket;
	BufferedSocket socket;

	std::exception_ptr error;

public:
	[[nodiscard]]
	explicit NetCat(EventLoop &event_loop) noexcept
		:connect_socket(event_loop, *this), socket(event_loop)
	{
	}

	auto &GetEventLoop() const noexcept {
		return socket.GetEventLoop();
	}

	void Start(SocketAddress address) noexcept {
		connect_socket.Connect(address, std::chrono::seconds{10});
	}

	void Finish() {
		if (error)
			std::rethrow_exception(error);
	}

private:
	/* virtual methods from class ConnectSocketHandler */
	void OnSocketConnectSuccess(UniqueSocketDescriptor fd) noexcept override {
		socket.Init(fd.Release(), FdType::FD_TCP, std::chrono::minutes{1}, *this);
		socket.EnableUring(*GetEventLoop().GetUring());
	}

	void OnSocketConnectError(std::exception_ptr e) noexcept override {
		GetEventLoop().SetVolatile();
		error = std::move(e);
	}

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

	bool OnBufferedEnd() override {
		GetEventLoop().SetVolatile();
		return true;
	}

	bool OnBufferedWrite() override {
		return true;
	}

	void OnBufferedError(std::exception_ptr e) noexcept override {
		GetEventLoop().SetVolatile();
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

	const auto addresses = Resolve(argv[1], 80, &hints);

	EventLoop event_loop;
	event_loop.EnableUring(1024, IORING_SETUP_SINGLE_ISSUER|IORING_SETUP_COOP_TASKRUN);

	NetCat net_cat{event_loop};

	net_cat.Start(addresses.GetBest());

	event_loop.Run();

	net_cat.Finish();

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
