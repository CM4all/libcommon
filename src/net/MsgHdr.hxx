/*
 * Copyright 2007-2020 CM4all GmbH
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

#include "SocketAddress.hxx"
#include "StaticSocketAddress.hxx"
#include "util/ConstBuffer.hxx"

#include <sys/socket.h>

inline constexpr struct msghdr
MakeMsgHdr(ConstBuffer<struct iovec> iov) noexcept
{
	struct msghdr mh{};
	mh.msg_iov = const_cast<struct iovec *>(iov.data);
	mh.msg_iovlen = iov.size;
	return mh;
}

/**
 * Construct a struct msghdr.  The parameters are `const` because that
 * is needed for sending; but for receiving, these buffers must
 * actually be writable.
 */
inline constexpr struct msghdr
MakeMsgHdr(SocketAddress name, ConstBuffer<struct iovec> iov,
	   ConstBuffer<void> control) noexcept
{
	auto mh = MakeMsgHdr(iov);
	mh.msg_name = const_cast<struct sockaddr *>(name.GetAddress());
	mh.msg_namelen = name.GetSize();
	mh.msg_control = const_cast<void *>(control.data);
	mh.msg_controllen = control.size;
	return mh;
}

inline constexpr struct msghdr
MakeMsgHdr(StaticSocketAddress name, ConstBuffer<struct iovec> iov,
	   ConstBuffer<void> control) noexcept
{
	auto mh = MakeMsgHdr(iov);
	mh.msg_name = name;
	mh.msg_namelen = name.GetCapacity();
	mh.msg_control = const_cast<void *>(control.data);
	mh.msg_controllen = control.size;
	return mh;
}

inline constexpr struct msghdr
MakeMsgHdr(struct sockaddr_storage &name, ConstBuffer<struct iovec> iov,
	   ConstBuffer<void> control) noexcept
{
	auto mh = MakeMsgHdr(iov);
	mh.msg_name = static_cast<struct sockaddr *>(static_cast<void *>(&name));
	mh.msg_namelen = sizeof(name);
	mh.msg_control = const_cast<void *>(control.data);
	mh.msg_controllen = control.size;
	return mh;
}
