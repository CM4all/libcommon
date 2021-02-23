/*
 * Copyright 2007-2021 CM4all GmbH
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

#pragma once

#include "Poll.hxx"
#include "event/CoarseTimerEvent.hxx"
#include "io/Logger.hxx"

#include <avahi-client/client.h>
#include <avahi-client/publish.h>

#include <string>
#include <forward_list>

class EventLoop;
class SocketAddress;

namespace Avahi {

class ConnectionListener;

class Client final {
	const LLogger logger;

	std::string name;

	CoarseTimerEvent reconnect_timer;

	Poll poll;

	AvahiClient *client = nullptr;
	AvahiEntryGroup *group = nullptr;

	struct Service {
		AvahiIfIndex interface;
		AvahiProtocol protocol;
		std::string type;
		uint16_t port;

		Service(AvahiIfIndex _interface, AvahiProtocol _protocol,
			const char *_type, uint16_t _port)
			:interface(_interface), protocol(_protocol),
			 type(_type), port(_port) {}
	};

	std::forward_list<Service> services;

	std::forward_list<ConnectionListener *> listeners;

	/**
	 * Shall the published services be visible?  This is controlled by
	 * HideServices() and ShowServices().
	 */
	bool visible_services = true;

public:
	Client(EventLoop &event_loop, const char *_name) noexcept;
	~Client() noexcept;

	Client(const Client &) = delete;
	Client &operator=(const Client &) = delete;

	EventLoop &GetEventLoop() const noexcept {
		return poll.GetEventLoop();
	}

	void Close() noexcept;

	void Activate() noexcept;

	void AddListener(ConnectionListener &listener) noexcept {
		listeners.push_front(&listener);
	}

	void RemoveListener(ConnectionListener &listener) noexcept {
		listeners.remove(&listener);
	}

	void AddService(AvahiIfIndex interface, AvahiProtocol protocol,
			const char *type, uint16_t port) noexcept;

	/**
	 * @param v6only the value of IPV6_V6ONLY (if this describes
	 * an IPv6 address)
	 */
	void AddService(const char *type, const char *interface,
			SocketAddress address, bool v6only) noexcept;

	/**
	 * Temporarily hide all registered services.  You can undo this
	 * with ShowServices().
	 */
	void HideServices() noexcept;

	/**
	 * Undo HideServices().
	 */
	void ShowServices() noexcept;

private:
	void GroupCallback(AvahiEntryGroup *g,
			   AvahiEntryGroupState state) noexcept;
	static void GroupCallback(AvahiEntryGroup *g,
				  AvahiEntryGroupState state,
				  void *userdata) noexcept;

	void RegisterServices(AvahiClient *c) noexcept;

	void ClientCallback(AvahiClient *c, AvahiClientState state) noexcept;
	static void ClientCallback(AvahiClient *c, AvahiClientState state,
				   void *userdata) noexcept;

	void OnReconnectTimer() noexcept;
};

} // namespace Avahi
