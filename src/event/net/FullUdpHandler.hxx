/*
 * Copyright 2007-2019 Content Management AG
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

#include <exception>

#include <stddef.h>

template<typename T> struct ConstBuffer;
template<typename T> struct WritableBuffer;
class SocketAddress;
class UniqueFileDescriptor;

/**
 * Handler for a #UdpListener.
 *
 * This is a class for a smooth API transition away from #UdpHandler
 * to an interface which allows receiving file descriptors.
 */
class FullUdpHandler {
public:
	/**
	 * Exceptions thrown by this method will be passed to OnUdpError().
	 *
	 * @param uid the peer process uid, or -1 if unknown
	 * @return false if the #UdpHandler was destroyed inside this method
	 */
	virtual bool OnUdpDatagram(ConstBuffer<void> payload,
				   WritableBuffer<UniqueFileDescriptor> fds,
				   SocketAddress address, int uid) = 0;

	/**
	 * An I/O error has occurred, and the socket is defunct.
	 * After returning, it is assumed that the #UdpListener has
	 * been destroyed.
	 */
	virtual void OnUdpError(std::exception_ptr ep) noexcept = 0;
};
