// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "net/ReceiveMessage.hxx"
#include "net/ScmRightsBuilder.hxx"
#include "net/SendMessage.hxx"
#include "net/SocketPair.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "system/linux/kcmp.h"
#include "io/Pipe.hxx"

#include <gtest/gtest.h>

[[gnu::pure]]
static bool
IsSame(FileDescriptor a, FileDescriptor b) noexcept
{
	return kcmp(getpid(), getpid(), KCMP_FILE, a.Get(), b.Get()) == 0;
}

/**
 * Send a datagram with an empty payload, but with a SCM_RIGHTS
 * control message.  The kernel installs those file descriptors in our
 * file table even though recvmsg() returns 0, so ReceiveMessage() must
 * adopt them; else they would be leaked.
 */
TEST(ReceiveMessage, EmptyPayloadWithFD)
{
	auto [a, b] = CreateSocketPairNonBlock(SOCK_SEQPACKET);

	auto [p1, p2] = CreatePipe();

	/* a datagram with no payload at all */
	MessageHeader msg{std::span<const struct iovec>{}};

	ScmRightsBuilder<1> b1{msg};
	b1.push_back(p1.Get());
	b1.Finish(msg);

	SendMessage(a, msg, 0);

	ReceiveMessageBuffer<256, CMSG_SPACE(sizeof(int))> buffer;
	auto result = ReceiveMessage(b, buffer, 0);

	EXPECT_TRUE(result.payload.empty());
	ASSERT_EQ(result.fds.size(), 1U);
	EXPECT_TRUE(IsSame(result.fds.front(), p1));
	EXPECT_FALSE(IsSame(result.fds.front(), p2));
}
