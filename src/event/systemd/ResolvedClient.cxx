// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "ResolvedClient.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "event/SocketEvent.hxx"
#include "system/Error.hxx"
#include "net/AllocatedSocketAddress.hxx"
#include "net/ConnectSocket.hxx"
#include "net/SocketProtocolError.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "net/IPv4Address.hxx"
#include "net/IPv6Address.hxx"
#include "util/IntrusiveList.hxx"
#include "util/Cancellable.hxx"
#include "util/SpanCast.hxx"

#include <boost/json.hpp>

#include <cassert>

namespace Systemd {

using std::string_view_literals::operator""sv;

static boost::json::object
JsonResolveHostname(std::string_view hostname)
{
	return {
		{"method"sv, "io.systemd.Resolve.ResolveHostname"sv},
		{"parameters"sv, {
				{"name"sv, hostname},
				{"flags"sv, 0},
			},
		},
	};
}

static std::string
SerializeResolveHostname(std::string_view hostname)
{
	std::string s = boost::json::serialize(JsonResolveHostname(hostname));
	s.push_back('\0');
	return s;
}

class ResolveHostnameRequest final : Cancellable {
	SocketEvent socket;
	const uint_least16_t port;
	ResolveHostnameHandler &handler;

public:
	ResolveHostnameRequest(EventLoop &event_loop, UniqueSocketDescriptor &&_socket,
			       uint_least16_t _port, ResolveHostnameHandler &_handler) noexcept
		:socket(event_loop, BIND_THIS_METHOD(OnSocketReady), _socket.Release()),
		 port(_port), handler(_handler)
	{
	}

	~ResolveHostnameRequest() noexcept {
		socket.Close();
	}

	void Start(std::string_view hostname,
		   CancellablePointer &cancel_ptr) {
		auto nbytes = socket.GetSocket().Send(AsBytes(SerializeResolveHostname(hostname)));
		if (nbytes < 0)
			throw MakeErrno("Failed to send");

		socket.ScheduleRead();
		cancel_ptr = *this;
	}

private:
	void OnResponse(std::string_view s);
	void OnSocketReady(unsigned events) noexcept;

	/* virtual methods from Cancellable */
	void Cancel() noexcept override {
		delete this;
	}
};

static IPv4Address
ToIPv4Address(const boost::json::array &j, uint_least16_t port)
{
	if (j.size() != 4)
		throw SocketProtocolError{"Malformed IPv4 address"};

	return {
		static_cast<uint8_t>(j.at(0).as_int64()),
		static_cast<uint8_t>(j.at(1).as_int64()),
		static_cast<uint8_t>(j.at(2).as_int64()),
		static_cast<uint8_t>(j.at(3).as_int64()),
		port,
	};
}

static IPv6Address
ToIPv6Address(const boost::json::array &j, uint_least16_t port,
	      uint_least32_t ifindex)
{
	if (j.size() != 16)
		throw SocketProtocolError{"Malformed IPv6 address"};

	return {
		static_cast<uint_least16_t>((j.at(0).as_int64() << 8) |
					    j.at(1).as_int64()),
		static_cast<uint_least16_t>((j.at(2).as_int64() << 8) |
					    j.at(3).as_int64()),
		static_cast<uint_least16_t>((j.at(4).as_int64() << 8) |
					    j.at(5).as_int64()),
		static_cast<uint_least16_t>((j.at(6).as_int64() << 8) |
					    j.at(7).as_int64()),
		static_cast<uint_least16_t>((j.at(8).as_int64() << 8) |
					    j.at(9).as_int64()),
		static_cast<uint_least16_t>((j.at(10).as_int64() << 8) |
					    j.at(11).as_int64()),
		static_cast<uint_least16_t>((j.at(12).as_int64() << 8) |
					    j.at(13).as_int64()),
		static_cast<uint_least16_t>((j.at(14).as_int64() << 8) |
					    j.at(15).as_int64()),
		port,
		ifindex,
	};
}

inline void
ResolveHostnameRequest::OnResponse(std::string_view s)
{
	assert(!s.empty());

	if (s.back() != '\0')
		throw SocketProtocolError("Malformed response");

	s.remove_suffix(1);

	const auto j = boost::json::parse(s).as_object();

	if (const auto *error = j.if_contains("error"sv))
		throw FmtRuntimeError("systemd-resolved error: {}",
				      boost::json::serialize(*error));

	auto &a = j.at("parameters"sv).at("addresses"sv).at(0).as_object();

	const auto &address = a.at("address"sv);

	uint_least32_t ifindex = 0;
	if (const auto *i = a.if_contains("ifindex"sv))
		ifindex = i->as_int64();

	switch (a.at("family"sv).as_int64()) {
	case AF_INET:
		handler.OnResolveHostname(ToIPv4Address(address.as_array(),
							port));
		break;

	case AF_INET6:
		handler.OnResolveHostname(ToIPv6Address(address.as_array(),
							port, ifindex));
		break;

	default:
		throw SocketProtocolError{"Unsupported address family"};
	}
}

void
ResolveHostnameRequest::OnSocketReady(unsigned events) noexcept
try {
	if (events & (socket.ERROR|socket.HANGUP))
		throw SocketClosedPrematurelyError{};

	char buffer[4096];
	auto nbytes = socket.GetSocket().Receive(std::as_writable_bytes(std::span{buffer}));
	if (nbytes < 0)
		throw MakeErrno("Failed to receive");

	socket.Close();

	if (nbytes == 0)
		throw SocketClosedPrematurelyError{};

	OnResponse(ToStringView(std::span{buffer}.first(nbytes)));
	delete this;
} catch (...) {
	auto &_handler = handler;
	delete this;
	_handler.OnResolveHostnameError(std::current_exception());
}

static UniqueSocketDescriptor
ConnectResolved()
{
	AllocatedSocketAddress address;
	address.SetLocal("/run/systemd/resolve/io.systemd.Resolve");
	return CreateConnectSocket(address, SOCK_STREAM);
}

void
ResolveHostname(EventLoop &event_loop,
		std::string_view hostname, unsigned port,
		ResolveHostnameHandler &handler,
		CancellablePointer &cancel_ptr) noexcept
try {
	auto *request = new ResolveHostnameRequest(event_loop, ConnectResolved(),
						   port, handler);

	try {
		request->Start(hostname, cancel_ptr);
	} catch (...) {
		delete request;
		throw;
	}
} catch (...) {
	handler.OnResolveHostnameError(std::current_exception());
}

} // namespace Systemd
