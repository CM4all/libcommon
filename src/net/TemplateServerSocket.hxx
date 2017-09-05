/*
 * Copyright 2007-2017 Content Management AG
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TEMPLATE_SERVER_SOCKET_HXX
#define TEMPLATE_SERVER_SOCKET_HXX

#include "ServerSocket.hxx"
#include "SocketAddress.hxx"
#include "util/PrintException.hxx"

#include <boost/intrusive/list.hpp>

#include <tuple>

class SocketAddress;

template<class C, std::size_t i>
struct ApplyTuple {
	template<typename T, typename... P>
	static C *Create(UniqueSocketDescriptor &&fd, T &&tuple, P&&... params) {
		return ApplyTuple<C, i - 1>::Create(std::move(fd),
						    std::forward<T>(tuple),
						    std::get<i - 1>(std::forward<T>(tuple)),
						    std::forward<P>(params)...);
	}
};

template<class C>
struct ApplyTuple<C, 0> {
	template<typename T, typename... P>
	static C *Create(UniqueSocketDescriptor &&fd, T &&, P&&... params) {
		return new C(std::forward<P>(params)..., std::move(fd));
	}
};

/**
 * A #ServerSocket implementation that creates a new instance of the
 * given class for each connection.
 */
template<typename C, typename... Params>
class TemplateServerSocket : public ServerSocket {
	static_assert(std::is_base_of<boost::intrusive::list_base_hook<boost::intrusive::link_mode<boost::intrusive::auto_unlink>>,
		      C>::value,
		      "Must use list_base_hook<auto_unlink>");

	typedef std::tuple<Params...> Tuple;
	Tuple params;

	boost::intrusive::list<C,
			       boost::intrusive::constant_time_size<false>> connections;

public:
	template<typename... P>
	TemplateServerSocket(EventLoop &event_loop, P&&... _params)
		:ServerSocket(event_loop), params(std::forward<P>(_params)...) {}

	~TemplateServerSocket() {
		while (!connections.empty())
			delete &connections.front();
	}

protected:
	void OnAccept(UniqueSocketDescriptor &&_fd, SocketAddress) override {
		auto *c = CreateConnection(std::move(_fd));
		connections.push_front(*c);
	};

	void OnAcceptError(std::exception_ptr ep) override {
		PrintException(ep);
	}

private:
	C *CreateConnection(UniqueSocketDescriptor &&_fd) {
		return ApplyTuple<C, std::tuple_size<Tuple>::value>::template Create<Tuple &>(std::move(_fd), params);
	}
};

#endif
