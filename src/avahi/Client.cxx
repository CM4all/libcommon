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

#include "Client.hxx"
#include "ConnectionListener.hxx"

#include <avahi-common/error.h>
#include <avahi-common/malloc.h>

#include <cassert>

namespace Avahi {

Client::Client(EventLoop &event_loop) noexcept
	:logger("avahi"),
	 reconnect_timer(event_loop, BIND_THIS_METHOD(OnReconnectTimer)),
	 poll(event_loop)
{
}

Client::~Client() noexcept
{
	Close();
}

void
Client::Activate() noexcept
{
	assert(client == nullptr);

	reconnect_timer.Schedule({});
}

void
Client::Close() noexcept
{
	if (client != nullptr) {
		for (auto *l : listeners)
			l->OnAvahiDisconnect();

		avahi_client_free(client);
		client = nullptr;
	}

	reconnect_timer.Cancel();
}

void
Client::ClientCallback(AvahiClient *c, AvahiClientState state) noexcept
{
	int error;

	switch (state) {
	case AVAHI_CLIENT_S_RUNNING:
		for (auto *l : listeners)
			l->OnAvahiConnect(c);

		break;

	case AVAHI_CLIENT_FAILURE:
		error = avahi_client_errno(c);
		if (error == AVAHI_ERR_DISCONNECTED) {
			Close();

			reconnect_timer.Schedule(std::chrono::seconds(10));
		} else {
			logger(3, "Avahi client failed: ", avahi_strerror(error));
			reconnect_timer.Schedule(std::chrono::minutes(1));
		}

		for (auto *l : listeners)
			l->OnAvahiDisconnect();

		break;

	case AVAHI_CLIENT_S_COLLISION:
	case AVAHI_CLIENT_S_REGISTERING:
		for (auto *l : listeners)
			l->OnAvahiChanged();

		break;

	case AVAHI_CLIENT_CONNECTING:
		break;
	}
}

void
Client::ClientCallback(AvahiClient *c, AvahiClientState state,
		       void *userdata) noexcept
{
	auto &client = *(Client *)userdata;
	client.ClientCallback(c, state);
}

void
Client::OnReconnectTimer() noexcept
{
	int error;
	client = avahi_client_new(&poll, AVAHI_CLIENT_NO_FAIL,
				  ClientCallback, this,
				  &error);
	if (client == nullptr) {
		logger(3, "Failed to create avahi client: ",
		       avahi_strerror(error));
		reconnect_timer.Schedule(std::chrono::minutes(1));
		return;
	}
}

} // namespace Avahi
