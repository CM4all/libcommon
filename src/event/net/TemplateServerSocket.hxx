// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#ifndef TEMPLATE_SERVER_SOCKET_HXX
#define TEMPLATE_SERVER_SOCKET_HXX

#include "ServerSocket.hxx"
#include "net/SocketAddress.hxx"
#include "util/DeleteDisposer.hxx"
#include "util/IntrusiveList.hxx"
#include "util/PrintException.hxx"

#include <tuple>

class SocketAddress;

template<class C, std::size_t i>
struct ApplyTuple {
	template<typename T, typename... P>
	static C *Create(UniqueSocketDescriptor fd, SocketAddress address,
			 T &&tuple, P&&... params) {
		return ApplyTuple<C, i - 1>::Create(std::move(fd), address,
						    std::forward<T>(tuple),
						    std::get<i - 1>(std::forward<T>(tuple)),
						    std::forward<P>(params)...);
	}
};

template<class C>
struct ApplyTuple<C, 0> {
	template<typename T, typename... P>
	static C *Create(UniqueSocketDescriptor fd, SocketAddress address,
			 T &&, P&&... params) {
		return new C(std::forward<P>(params)...,
			     std::move(fd), address);
	}
};

/**
 * A #ServerSocket implementation that creates a new instance of the
 * given class for each connection.
 */
template<typename C, typename... Params>
class TemplateServerSocket final : public ServerSocket {
	static_assert(std::is_base_of_v<AutoUnlinkIntrusiveListHook, C>,
		      "Must use AutoUnlinkIntrusiveListHook");

	using Tuple = std::tuple<Params...>;
	Tuple params;

	IntrusiveList<C> connections;

public:
	template<typename... P>
	TemplateServerSocket(EventLoop &event_loop, P&&... _params)
		:ServerSocket(event_loop), params(std::forward<P>(_params)...) {}

	~TemplateServerSocket() noexcept {
		CloseAllConnections();
	}

	void CloseAllConnections() noexcept {
		connections.clear_and_dispose(DeleteDisposer{});
	}

protected:
	void OnAccept(UniqueSocketDescriptor &&_fd,
		      SocketAddress address) noexcept override {
		try {
			auto *c = CreateConnection(std::move(_fd), address);
			connections.push_front(*c);
		} catch (...) {
			OnAcceptError(std::current_exception());
		}
	}

	void OnAcceptError(std::exception_ptr ep) noexcept override {
		PrintException(ep);
	}

private:
	C *CreateConnection(UniqueSocketDescriptor _fd,
			    SocketAddress address) {
		return ApplyTuple<C, std::tuple_size<Tuple>::value>::template Create<Tuple &>(std::move(_fd),
											      address, params);
	}
};

#endif
