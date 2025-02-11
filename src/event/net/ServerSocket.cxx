// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "ServerSocket.hxx"
#include "net/IPv4Address.hxx"
#include "net/IPv6Address.hxx"
#include "net/LocalSocketAddress.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "net/StaticSocketAddress.hxx"
#include "net/SocketConfig.hxx"
#include "net/SocketError.hxx"

#ifdef HAVE_URING
#include "event/Loop.hxx"
#include "io/uring/Operation.hxx"
#include "io/uring/Queue.hxx"
#endif

#include <cassert>

#ifdef HAVE_URING

class ServerSocket::UringAccept final : Uring::Operation {
	ServerSocket &parent;
	Uring::Queue &queue;
	StaticSocketAddress remote_address;
	socklen_t remote_address_size;

	bool released = false;

public:
	UringAccept(ServerSocket &_parent, Uring::Queue &_queue) noexcept
		:parent(_parent), queue(_queue) {}

	auto &GetQueue() const noexcept {
		return queue;
	}

	void Release() noexcept {
		assert(!released);

		if (IsUringPending()) {
			if (auto *s = queue.GetSubmitEntry()) {
				io_uring_prep_cancel(s, GetUringData(), 0);
				io_uring_sqe_set_data(s, nullptr);
				io_uring_sqe_set_flags(s, IOSQE_CQE_SKIP_SUCCESS);
				queue.Submit();
			}

			released = true;
		} else
			delete this;
	}

	void Start();

private:
	void OnUringCompletion(int res) noexcept override;
};

inline void
ServerSocket::UringAccept::Start()
{
	assert(!released);

	if (IsUringPending())
		return;

	auto &s = queue.RequireSubmitEntry();

	remote_address_size = remote_address.GetCapacity();
	io_uring_prep_accept(&s, parent.GetSocket().Get(),
			     remote_address, &remote_address_size,
			     SOCK_NONBLOCK|SOCK_CLOEXEC);
	queue.Push(s, *this);
}

void
ServerSocket::UringAccept::OnUringCompletion(int res) noexcept
{
	if (released) [[unlikely]] {
		if (res >= 0)
			close(res);
		delete this;
		return;
	}

	if (res >= 0) [[likely]] {
		remote_address.SetSize(remote_address_size);
		parent.OnAccept(UniqueSocketDescriptor{AdoptTag{}, res}, remote_address);
		Start();
	} else {
		parent.OnAcceptError(std::make_exception_ptr(MakeSocketError(-res, "Failed to accept connection")));
	}
}

#endif

ServerSocket::~ServerSocket() noexcept
{
#ifdef HAVE_URING
	if (uring_accept != nullptr)
		uring_accept->Release();
#endif

	event.Close();
}

void
ServerSocket::Listen(UniqueSocketDescriptor _fd) noexcept
{
	assert(!event.IsDefined());
	assert(_fd.IsDefined());

	event.Open(_fd.Release());

#ifdef HAVE_URING
	assert(uring_accept == nullptr);

	if (auto *uring_queue = GetEventLoop().GetUring()) {
		uring_accept = new UringAccept(*this, *uring_queue);
		uring_accept->Start();
	} else
#endif
		event.ScheduleRead();
}

static UniqueSocketDescriptor
MakeListener(const SocketAddress address,
	     bool reuse_port,
	     bool free_bind,
	     const char *bind_to_device)
{
	constexpr int socktype = SOCK_STREAM;

	SocketConfig config{
		.bind_address = AllocatedSocketAddress{address},
		.listen = 256,
		.reuse_port = reuse_port,
		.free_bind = free_bind,
		.pass_cred = true,
	};

	if (bind_to_device != nullptr)
		config.interface = bind_to_device;

	return config.Create(socktype);
}

void
ServerSocket::Listen(SocketAddress address,
		     bool reuse_port,
		     bool free_bind,
		     const char *bind_to_device)
{
	Listen(MakeListener(address, reuse_port, free_bind, bind_to_device));
}

void
ServerSocket::ListenTCP(unsigned port)
{
	try {
		ListenTCP6(port);
	} catch (...) {
		ListenTCP4(port);
	}
}

void
ServerSocket::ListenTCP4(unsigned port)
{
	assert(port > 0);

	Listen(IPv4Address(port),
	       false, false, nullptr);
}

void
ServerSocket::ListenTCP6(unsigned port)
{
	assert(port > 0);

	Listen(IPv6Address(port),
	       false, false, nullptr);
}

void
ServerSocket::ListenPath(const char *path)
{
	Listen(LocalSocketAddress{path}, false, false, nullptr);
}

void
ServerSocket::EventCallback(unsigned) noexcept
{
	StaticSocketAddress remote_address;
	UniqueSocketDescriptor remote_fd{AdoptTag{}, GetSocket().AcceptNonBlock(remote_address)};
	if (!remote_fd.IsDefined()) {
		const auto e = GetSocketError();
		if (!IsSocketErrorAcceptWouldBlock(e))
			OnAcceptError(std::make_exception_ptr(MakeSocketError(e, "Failed to accept connection")));

		return;
	}

	OnAccept(std::move(remote_fd), remote_address);
}
