// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "SocketDescriptor.hxx"
#include "SocketAddress.hxx"
#include "StaticSocketAddress.hxx"
#include "IPv4Address.hxx"
#include "IPv6Address.hxx"
#include "UniqueSocketDescriptor.hxx"
#include "PeerCredentials.hxx"

#ifdef __linux__
#include "io/UniqueFileDescriptor.hxx"
#endif

#ifdef HAVE_GETPEEREID
#include <unistd.h> // for getpeereid()
#endif

#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include "MsgHdr.hxx"

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#endif

#include <cassert>
#include <cerrno>

#include <string.h>

int
SocketDescriptor::GetType() const noexcept
{
	return GetIntOption(SOL_SOCKET, SO_TYPE, -1);
}

bool
SocketDescriptor::IsStream() const noexcept
{
	return GetType() == SOCK_STREAM;
}

#ifdef  __linux__

int
SocketDescriptor::GetProtocol() const noexcept
{
	return GetIntOption(SOL_SOCKET, SO_PROTOCOL, -1);
}

#endif // __linux__

#ifdef _WIN32

void
SocketDescriptor::Close() noexcept
{
	if (IsDefined())
		::closesocket(Steal());
}

#endif

SocketDescriptor
SocketDescriptor::Accept() const noexcept
{
#ifdef __linux__
	int connection_fd = ::accept4(Get(), nullptr, nullptr, SOCK_CLOEXEC);
#else
	int connection_fd = ::accept(Get(), nullptr, nullptr);
#endif
	return connection_fd >= 0
		? SocketDescriptor(connection_fd)
		: Undefined();
}

SocketDescriptor
SocketDescriptor::AcceptNonBlock() const noexcept
{
#ifdef __linux__
	int connection_fd = ::accept4(Get(), nullptr, nullptr,
				      SOCK_CLOEXEC|SOCK_NONBLOCK);
#else
	int connection_fd = ::accept(Get(), nullptr, nullptr);
	if (connection_fd >= 0)
		SocketDescriptor(connection_fd).SetNonBlocking();
#endif
	return SocketDescriptor(connection_fd);
}

SocketDescriptor
SocketDescriptor::AcceptNonBlock(StaticSocketAddress &address) const noexcept
{
	address.SetMaxSize();
#ifdef __linux__
	int connection_fd = ::accept4(Get(), address, &address.size,
				      SOCK_CLOEXEC|SOCK_NONBLOCK);
#else
	int connection_fd = ::accept(Get(), address, &address.size);
	if (connection_fd >= 0)
		SocketDescriptor(connection_fd).SetNonBlocking();
#endif
	return SocketDescriptor(connection_fd);
}

bool
SocketDescriptor::Connect(SocketAddress address) const noexcept
{
	assert(address.IsDefined());

	return ::connect(Get(), address.GetAddress(), address.GetSize()) >= 0;
}

bool
SocketDescriptor::Create(int domain, int type, int protocol) noexcept
{
#ifdef _WIN32
	static bool initialised = false;
	if (!initialised) {
		WSADATA data;
		WSAStartup(MAKEWORD(2,2), &data);
		initialised = true;
	}
#endif

#ifdef SOCK_CLOEXEC
	/* implemented since Linux 2.6.27 */
	type |= SOCK_CLOEXEC;
#endif

	int new_fd = socket(domain, type, protocol);
	if (new_fd < 0)
		return false;

	Set(new_fd);
	return true;
}

bool
SocketDescriptor::CreateNonBlock(int domain, int type, int protocol) noexcept
{
#ifdef SOCK_NONBLOCK
	type |= SOCK_NONBLOCK;
#endif

	if (!Create(domain, type, protocol))
		return false;

#ifndef SOCK_NONBLOCK
	SetNonBlocking();
#endif

	return true;
}

#ifndef _WIN32

bool
SocketDescriptor::CreateSocketPair(int domain, int type, int protocol,
				   SocketDescriptor &a,
				   SocketDescriptor &b) noexcept
{
#ifdef SOCK_CLOEXEC
	/* implemented since Linux 2.6.27 */
	type |= SOCK_CLOEXEC;
#endif

	int fds[2];
	if (socketpair(domain, type, protocol, fds) < 0)
		return false;

	a = SocketDescriptor(fds[0]);
	b = SocketDescriptor(fds[1]);
	return true;
}

bool
SocketDescriptor::CreateSocketPairNonBlock(int domain, int type, int protocol,
					   SocketDescriptor &a,
					   SocketDescriptor &b) noexcept
{
#ifdef SOCK_NONBLOCK
	type |= SOCK_NONBLOCK;
#endif

	if (!CreateSocketPair(domain, type, protocol, a, b))
		return false;

#ifndef SOCK_NONBLOCK
	a.SetNonBlocking();
	b.SetNonBlocking();
#endif

	return true;
}

#endif

int
SocketDescriptor::GetError() const noexcept
{
	int s_err = 0;
	return GetOption(SOL_SOCKET, SO_ERROR,
			 &s_err, sizeof(s_err)) == sizeof(s_err)
		? s_err
		: errno;
}

std::size_t
SocketDescriptor::GetOption(int level, int name,
			    void *value, std::size_t size) const noexcept
{
	assert(IsDefined());

	socklen_t size2 = size;
	return getsockopt(fd, level, name, (char *)value, &size2) == 0
		? size2
		: 0;
}

int
SocketDescriptor::GetIntOption(int level, int name, int fallback) const noexcept
{
	int value = fallback;
	(void)GetOption(level, name, &value, sizeof(value));
	return value;
}

SocketPeerCredentials
SocketDescriptor::GetPeerCredentials() const noexcept
{
#ifdef HAVE_STRUCT_UCRED
	SocketPeerCredentials cred;
	if (GetOption(SOL_SOCKET, SO_PEERCRED,
		      &cred.cred, sizeof(cred.cred)) < sizeof(cred.cred))
		return SocketPeerCredentials::Undefined();
	return cred;
#elif defined(HAVE_GETPEEREID)
	SocketPeerCredentials cred;
	return getpeereid(Get(), &cred.uid, &cred.gid) == 0
		? cred
		: SocketPeerCredentials::Undefined();
#else
	return SocketPeerCredentials::Undefined();
#endif
}

#ifdef __linux__

#ifndef SO_PEERPIDFD
#define SO_PEERPIDFD 77
#endif

UniqueFileDescriptor
SocketDescriptor::GetPeerPidfd() const noexcept
{
	int pidfd;
	if (GetOption(SOL_SOCKET, SO_PEERPIDFD, &pidfd, sizeof(pidfd)) < sizeof(pidfd))
		return {};

	return UniqueFileDescriptor{AdoptTag{}, pidfd};
}

#endif // __linux__

#ifdef _WIN32

bool
SocketDescriptor::SetNonBlocking() const noexcept
{
	u_long val = 1;
	return ioctlsocket(fd, FIONBIO, &val) == 0;
}

#else

UniqueSocketDescriptor
SocketDescriptor::Duplicate() const noexcept
{
	return UniqueSocketDescriptor{FileDescriptor::Duplicate()};
}

#endif // !_WIN32

bool
SocketDescriptor::SetOption(int level, int name,
			    const void *value, std::size_t size) const noexcept
{
	assert(IsDefined());

	/* on Windows, setsockopt() wants "const char *" */
	return setsockopt(fd, level, name, (const char *)value, size) == 0;
}

bool
SocketDescriptor::SetKeepAlive(bool value) const noexcept
{
	return SetBoolOption(SOL_SOCKET, SO_KEEPALIVE, value);
}

bool
SocketDescriptor::SetReuseAddress(bool value) const noexcept
{
	return SetBoolOption(SOL_SOCKET, SO_REUSEADDR, value);
}

#ifdef __linux__

bool
SocketDescriptor::SetReusePort(bool value) const noexcept
{
	return SetBoolOption(SOL_SOCKET, SO_REUSEPORT, value);
}

bool
SocketDescriptor::SetFreeBind(bool value) const noexcept
{
	return SetBoolOption(IPPROTO_IP, IP_FREEBIND, value);
}

bool
SocketDescriptor::SetNoDelay(bool value) const noexcept
{
	return SetBoolOption(IPPROTO_TCP, TCP_NODELAY, value);
}

bool
SocketDescriptor::SetCork(bool value) const noexcept
{
	return SetBoolOption(IPPROTO_TCP, TCP_CORK, value);
}

bool
SocketDescriptor::SetTcpDeferAccept(const int &seconds) const noexcept
{
	return SetOption(IPPROTO_TCP, TCP_DEFER_ACCEPT, &seconds, sizeof(seconds));
}

bool
SocketDescriptor::SetTcpUserTimeout(const unsigned &milliseconds) const noexcept
{
	return SetOption(IPPROTO_TCP, TCP_USER_TIMEOUT,
			 &milliseconds, sizeof(milliseconds));
}

bool
SocketDescriptor::SetV6Only(bool value) const noexcept
{
	return SetBoolOption(IPPROTO_IPV6, IPV6_V6ONLY, value);
}

bool
SocketDescriptor::SetBindToDevice(const char *name) const noexcept
{
	return SetOption(SOL_SOCKET, SO_BINDTODEVICE, name, strlen(name));
}

#ifdef TCP_FASTOPEN

bool
SocketDescriptor::SetTcpFastOpen(int qlen) const noexcept
{
	return SetOption(SOL_TCP, TCP_FASTOPEN, &qlen, sizeof(qlen));
}

#endif

#ifdef HAVE_TCP

bool
SocketDescriptor::AddMembership(const IPv4Address &address) const noexcept
{
	struct ip_mreq r{address.GetAddress(), IPv4Address(0).GetAddress()};
	return setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
			  &r, sizeof(r)) == 0;
}

#ifdef HAVE_IPV6

bool
SocketDescriptor::AddMembership(const IPv6Address &address) const noexcept
{
	struct ipv6_mreq r{address.GetAddress(), 0};
	r.ipv6mr_interface = address.GetScopeId();
	return setsockopt(fd, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP,
			  &r, sizeof(r)) == 0;
}

#endif // HAVE_IPV6

bool
SocketDescriptor::AddMembership(SocketAddress address) const noexcept
{
	switch (address.GetFamily()) {
	case AF_INET:
		return AddMembership(IPv4Address(address));

#ifdef HAVE_IPV6
	case AF_INET6:
		return AddMembership(IPv6Address(address));
#endif

	default:
		errno = EINVAL;
		return false;
	}
}

#endif // HAVE_TCP

#endif // __linux__

bool
SocketDescriptor::Bind(SocketAddress address) const noexcept
{
	return bind(Get(), address.GetAddress(), address.GetSize()) == 0;
}

#ifdef __linux__

bool
SocketDescriptor::AutoBind() const noexcept
{
	static constexpr sa_family_t family = AF_LOCAL;
	return Bind(SocketAddress((const struct sockaddr *)&family,
				  sizeof(family)));
}

#endif

bool
SocketDescriptor::Listen(int backlog) const noexcept
{
	return listen(Get(), backlog) == 0;
}

StaticSocketAddress
SocketDescriptor::GetLocalAddress() const noexcept
{
	assert(IsDefined());

	StaticSocketAddress result;
	result.size = result.GetCapacity();
	if (getsockname(fd, result, &result.size) < 0)
		result.Clear();

	return result;
}

StaticSocketAddress
SocketDescriptor::GetPeerAddress() const noexcept
{
	assert(IsDefined());

	StaticSocketAddress result;
	result.size = result.GetCapacity();
	if (getpeername(fd, result, &result.size) < 0)
		result.Clear();

	return result;
}

ssize_t
SocketDescriptor::Receive(std::span<std::byte> dest, int flags) const noexcept
{
	return ::recv(Get(), (char *)dest.data(), dest.size(), flags);
}

#ifndef _WIN32

ssize_t
SocketDescriptor::Receive(struct msghdr &msg, int flags) const noexcept
{
	return ::recvmsg(Get(), &msg, flags);
}

ssize_t
SocketDescriptor::Receive(std::span<const struct iovec> v, int flags) const noexcept
{
	auto msg = MakeMsgHdr(v);
	return Receive(msg, flags);
}

#endif // !_WIN32

ssize_t
SocketDescriptor::Send(std::span<const std::byte> src, int flags) const noexcept
{
#ifdef __linux__
	flags |= MSG_NOSIGNAL;
#endif

	return ::send(Get(), (const char *)src.data(), src.size(), flags);
}

#ifndef _WIN32

ssize_t
SocketDescriptor::Send(const struct msghdr &msg, int flags) const noexcept
{
#ifdef __linux__
	flags |= MSG_NOSIGNAL;
#endif

	return ::sendmsg(Get(), &msg, flags);
}

ssize_t
SocketDescriptor::Send(std::span<const struct iovec> v, int flags) const noexcept
{
	return Send(MakeMsgHdr(v), flags);
}

#endif // !_WIN32

ssize_t
SocketDescriptor::ReadNoWait(std::span<std::byte> dest) const noexcept
{
	int flags = 0;
#ifndef _WIN32
	flags |= MSG_DONTWAIT;
#endif

	return Receive(dest, flags);
}

ssize_t
SocketDescriptor::WriteNoWait(std::span<const std::byte> src) const noexcept
{
	int flags = 0;
#ifndef _WIN32
	flags |= MSG_DONTWAIT;
#endif
#ifdef __linux__
	flags |= MSG_NOSIGNAL;
#endif

	return Send(src, flags);
}

#ifdef _WIN32

int
SocketDescriptor::WaitReadable(int timeout_ms) const noexcept
{
	assert(IsDefined());

	fd_set rfds;
	FD_ZERO(&rfds);
	FD_SET(Get(), &rfds);

	struct timeval timeout, *timeout_p = nullptr;
	if (timeout_ms >= 0) {
		timeout.tv_sec = unsigned(timeout_ms) / 1000;
		timeout.tv_usec = (unsigned(timeout_ms) % 1000) * 1000;
		timeout_p = &timeout;
	}

	return select(Get() + 1, &rfds, nullptr, nullptr, timeout_p);
}

int
SocketDescriptor::WaitWritable(int timeout_ms) const noexcept
{
	assert(IsDefined());

	fd_set wfds;
	FD_ZERO(&wfds);
	FD_SET(Get(), &wfds);

	struct timeval timeout, *timeout_p = nullptr;
	if (timeout_ms >= 0) {
		timeout.tv_sec = unsigned(timeout_ms) / 1000;
		timeout.tv_usec = (unsigned(timeout_ms) % 1000) * 1000;
		timeout_p = &timeout;
	}

	return select(Get() + 1, nullptr, &wfds, nullptr, timeout_p);
}

#endif

ssize_t
SocketDescriptor::ReadNoWait(std::span<std::byte> dest,
			     StaticSocketAddress &address) const noexcept
{
	int flags = 0;
#ifndef _WIN32
	flags |= MSG_DONTWAIT;
#endif

	socklen_t addrlen = address.GetCapacity();
	ssize_t nbytes = ::recvfrom(Get(),
				    reinterpret_cast<char *>(dest.data()),
				    dest.size(), flags,
				    address, &addrlen);
	if (nbytes > 0)
		address.SetSize(addrlen);

	return nbytes;
}

ssize_t
SocketDescriptor::WriteNoWait(std::span<const std::byte> src,
			      SocketAddress address) const noexcept
{
	int flags = 0;
#ifndef _WIN32
	flags |= MSG_DONTWAIT;
#endif
#ifdef __linux__
	flags |= MSG_NOSIGNAL;
#endif

	return ::sendto(Get(), reinterpret_cast<const char *>(src.data()),
			src.size(), flags,
			address.GetAddress(), address.GetSize());
}

#ifndef _WIN32

void
SocketDescriptor::Shutdown() const noexcept
{
    shutdown(Get(), SHUT_RDWR);
}

void
SocketDescriptor::ShutdownRead() const noexcept
{
    shutdown(Get(), SHUT_RD);
}

void
SocketDescriptor::ShutdownWrite() const noexcept
{
    shutdown(Get(), SHUT_WR);
}

#endif
