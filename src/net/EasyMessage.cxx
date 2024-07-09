// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "EasyMessage.hxx"
#include "ReceiveMessage.hxx"
#include "SendMessage.hxx"
#include "ScmRightsBuilder.hxx"
#include "SocketDescriptor.hxx"
#include "SocketProtocolError.hxx"
#include "io/Iovec.hxx"
#include "util/Exception.hxx"
#include "util/SpanCast.hxx"

#include <cstdint>

static constexpr std::byte SUCCESS{};
static constexpr std::byte ERROR{1};

void
EasySendMessage(SocketDescriptor s, std::span<const std::byte> payload,
		FileDescriptor fd)
{
	const struct iovec v[] = {MakeIovec(payload)};
	MessageHeader msg{v};

	ScmRightsBuilder<1> srb(msg);
	if (fd.IsDefined())
		srb.push_back(fd.Get());
	srb.Finish(msg);

	SendMessage(s, msg, MSG_NOSIGNAL);
}

void
EasySendMessage(SocketDescriptor s, FileDescriptor fd)
{
	EasySendMessage(s, ReferenceAsBytes(SUCCESS), fd);
}

void
EasySendError(SocketDescriptor s, std::string_view text)
{
	const struct iovec v[] = {MakeIovecT(ERROR), MakeIovec(AsBytes(text))};
	MessageHeader msg{v};
	SendMessage(s, msg, MSG_NOSIGNAL);
}

void
EasySendError(SocketDescriptor s, std::exception_ptr error)
{
	// TODO add special case for std:;system_error / errno
	EasySendError(s, GetFullMessage(error));
}

UniqueFileDescriptor
EasyReceiveMessageWithOneFD(SocketDescriptor s)
{
	ReceiveMessageBuffer<256, 4> buffer;
	auto d = ReceiveMessage(s, buffer, 0);
	if (d.payload.empty())
		throw SocketClosedPrematurelyError{};

	if (d.fds.empty()) {
		if (d.payload.size() > 1 && d.payload.front() == ERROR)
			throw std::runtime_error{std::string{ToStringView(d.payload.subspan(1))}};

		return {};
	}

	return std::move(d.fds.front());
}
