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

#pragma once

#include "net/UniqueSocketDescriptor.hxx"
#include "event/SocketEvent.hxx"
#include "event/TimerEvent.hxx"
#include "util/Cancellable.hxx"

#include <exception>

class Error;
class SocketAddress;
class AddressInfo;

class ConnectSocketHandler {
public:
	virtual void OnSocketConnectSuccess(UniqueSocketDescriptor &&fd) noexcept = 0;
	virtual void OnSocketConnectTimeout() noexcept;
	virtual void OnSocketConnectError(std::exception_ptr ep) noexcept = 0;
};

/**
 * A class that connects to a SocketAddress.
 */
class ConnectSocket final : public Cancellable {
	ConnectSocketHandler &handler;

	UniqueSocketDescriptor fd;

	SocketEvent event;
	TimerEvent timeout_event;

public:
	ConnectSocket(EventLoop &_event_loop,
		      ConnectSocketHandler &_handler) noexcept;
	~ConnectSocket() noexcept;

	bool IsPending() const noexcept {
		return fd.IsDefined();
	}

	bool Connect(SocketAddress address,
		     const struct timeval &timeout) noexcept;

	bool Connect(SocketAddress address) noexcept;

	bool Connect(const AddressInfo &address,
		     const struct timeval *timeout) noexcept;

	bool Connect(const AddressInfo &address,
		     const struct timeval &timeout) noexcept {
		return Connect(address, &timeout);
	}

	/**
	 * Wait until the given socket is connected (this method returns
	 * immediately and invokes the #ConnectSocketHandler on completion
	 * or error).
	 */
	void WaitConnected(UniqueSocketDescriptor _fd,
			   const struct timeval *timeout) noexcept;

	void WaitConnected(UniqueSocketDescriptor _fd,
			   const struct timeval &timeout) noexcept {
		WaitConnected(std::move(_fd), &timeout);
	}

	/* virtual methods from Cancellable */
	void Cancel() noexcept override;

private:
	void OnEvent(unsigned events) noexcept;
	void OnTimeout() noexcept;
};
