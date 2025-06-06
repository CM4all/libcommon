// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "SocketAddress.hxx"
#include "system/LargeAllocation.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "util/AllocatedArray.hxx"
#include "util/OffsetPointer.hxx"

#include <memory>
#include <span>

class SocketDescriptor;

/**
 * This class helps to receive many network datagrams from a socket
 * efficiently.  To do that, it allocates and manages buffers to be
 * used by recvmmsg().
 */
class MultiReceiveMessage {
	const size_t allocated_datagrams;
	const size_t max_payload_size, max_cmsg_size;
	size_t n_datagrams = 0;

	LargeAllocation buffer;

	AllocatedArray<UniqueFileDescriptor> fds;
	size_t n_fds = 0;

public:
	struct Datagram {
		SocketAddress address;
		std::span<std::byte> payload;
		const struct ucred *cred;
		std::span<UniqueFileDescriptor> fds;
	};

	typedef Datagram *iterator;

private:
	std::unique_ptr<Datagram[]> datagrams;

public:
	MultiReceiveMessage(size_t _allocated_datagrams,
			    size_t _max_payload_size,
			    size_t _max_cmsg_size=0,
			    size_t _max_fds=0);

	MultiReceiveMessage(MultiReceiveMessage &&) noexcept = default;
	MultiReceiveMessage &operator=(MultiReceiveMessage &&) noexcept = delete;

	/**
	 * Receive new datagrams.  Any previous ones will be discarded.
	 *
	 * Throws on error.
	 *
	 * @return false if the peer has closed the connection
	 */
	bool Receive(SocketDescriptor s);

	/**
	 * Discard all datagrams.  This should be called after
	 * processing with begin() and end() has finished to free
	 * resources such as file descriptors.
	 */
	void Clear();

	iterator begin() {
		return datagrams.get();
	}

	iterator end() {
		return datagrams.get() + n_datagrams;
	}

private:
	void *At(size_t offset) {
		return OffsetPointer(buffer.get(), offset);
	}

	void *GetPayload(size_t i) noexcept {
		return At(i * max_payload_size);
	}

	void *GetCmsg(size_t i) noexcept {
		return OffsetPointer(GetPayload(allocated_datagrams),
				     i * max_cmsg_size);
	}

	struct mmsghdr *GetMmsg() noexcept {
		return (struct mmsghdr *)GetCmsg(allocated_datagrams);
	}
};
