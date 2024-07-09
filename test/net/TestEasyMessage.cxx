// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "net/EasyMessage.hxx"
#include "net/SocketPair.hxx"
#include "net/SocketProtocolError.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "system/linux/kcmp.h"
#include "io/Pipe.hxx"

#include <gtest/gtest.h>

using std::string_view_literals::operator""sv;

[[gnu::pure]]
static bool
IsSame(FileDescriptor a, FileDescriptor b) noexcept
{
	return kcmp(getpid(), getpid(), KCMP_FILE, a.Get(), b.Get()) == 0;
}

TEST(EasyMessage, OneFD)
{
	auto [a, b] = CreateSocketPairNonBlock(SOCK_SEQPACKET);

	// throws EAGAIN
	EXPECT_THROW(EasyReceiveMessageWithOneFD(a), std::system_error);
	EXPECT_THROW(EasyReceiveMessageWithOneFD(b), std::system_error);

	// create a pipe and send both ends over the socket
	auto [p1, p2] = CreatePipe();
	EXPECT_TRUE(IsSame(p1, p1));
	EXPECT_TRUE(IsSame(p2, p2));
	EXPECT_FALSE(IsSame(p1, p2));
	EXPECT_FALSE(IsSame(p2, p1));

	EasySendMessage(a, p1);
	EasySendMessage(a, p2);

	EXPECT_THROW(EasyReceiveMessageWithOneFD(a), std::system_error);

	// receive and compare both pipes
	auto fd = EasyReceiveMessageWithOneFD(b);
	EXPECT_TRUE(fd.IsDefined());
	EXPECT_TRUE(IsSame(fd, p1));
	EXPECT_FALSE(IsSame(fd, p2));

	fd = EasyReceiveMessageWithOneFD(b);
	EXPECT_TRUE(fd.IsDefined());
	EXPECT_FALSE(IsSame(fd, p1));
	EXPECT_TRUE(IsSame(fd, p2));

	// throws EAGAIN
	EXPECT_THROW(EasyReceiveMessageWithOneFD(a), std::system_error);
	EXPECT_THROW(EasyReceiveMessageWithOneFD(b), std::system_error);

	// send message without a file descriptor
	EasySendMessage(b, FileDescriptor::Undefined());
	fd = EasyReceiveMessageWithOneFD(a);
	EXPECT_FALSE(fd.IsDefined());

	// throws EAGAIN
	EXPECT_THROW(EasyReceiveMessageWithOneFD(a), std::system_error);
	EXPECT_THROW(EasyReceiveMessageWithOneFD(b), std::system_error);

	// close one end
	a.Close();
	EXPECT_THROW(EasyReceiveMessageWithOneFD(b), SocketClosedPrematurelyError);
}

TEST(EasyMessage, Error)
{
	auto [a, b] = CreateSocketPairNonBlock(SOCK_SEQPACKET);

	// throws EAGAIN
	EXPECT_THROW(EasyReceiveMessageWithOneFD(a), std::system_error);
	EXPECT_THROW(EasyReceiveMessageWithOneFD(b), std::system_error);

	// send error, receive error
	EasySendError(a, "hello"sv);
	EXPECT_THROW(EasyReceiveMessageWithOneFD(b), std::runtime_error);

	// throws EAGAIN
	EXPECT_THROW(EasyReceiveMessageWithOneFD(a), std::system_error);
	EXPECT_THROW(EasyReceiveMessageWithOneFD(b), std::system_error);
}
