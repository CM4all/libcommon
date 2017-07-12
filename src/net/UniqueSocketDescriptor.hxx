/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef UNIQUE_SOCKET_DESCRIPTOR_SOCKET_HXX
#define UNIQUE_SOCKET_DESCRIPTOR_SOCKET_HXX

#include "SocketDescriptor.hxx"

#include <algorithm>
#include <utility>

class StaticSocketAddress;

/**
 * Wrapper for a socket file descriptor.
 */
class UniqueSocketDescriptor : public SocketDescriptor {
public:
	UniqueSocketDescriptor()
		:SocketDescriptor(SocketDescriptor::Undefined()) {}

	explicit UniqueSocketDescriptor(SocketDescriptor _fd)
		:SocketDescriptor(_fd) {}
	explicit UniqueSocketDescriptor(FileDescriptor _fd)
		:SocketDescriptor(_fd) {}
	explicit UniqueSocketDescriptor(int _fd):SocketDescriptor(_fd) {}

	UniqueSocketDescriptor(UniqueSocketDescriptor &&other)
		:SocketDescriptor(std::exchange(other.fd, -1)) {}

	~UniqueSocketDescriptor() {
		if (IsDefined())
			Close();
	}

	/**
	 * Release ownership and return the descriptor as an unmanaged
	 * #SocketDescriptor instance.
	 */
	SocketDescriptor Release() {
		return std::exchange(*(SocketDescriptor *)this, Undefined());
	}

	UniqueSocketDescriptor &operator=(UniqueSocketDescriptor &&src) {
		std::swap(fd, src.fd);
		return *this;
	}

	bool operator==(const UniqueSocketDescriptor &other) const {
		return fd == other.fd;
	}

	/**
	 * @return an "undefined" instance on error
	 */
	UniqueSocketDescriptor AcceptNonBlock(StaticSocketAddress &address) const {
		return UniqueSocketDescriptor(SocketDescriptor::AcceptNonBlock(address));
	}

	static bool CreateSocketPair(int domain, int type, int protocol,
				     UniqueSocketDescriptor &a,
				     UniqueSocketDescriptor &b) {
		return SocketDescriptor::CreateSocketPair(domain, type,
							  protocol,
							  a, b);
	}

	static bool CreateSocketPairNonBlock(int domain, int type, int protocol,
					     UniqueSocketDescriptor &a,
					     UniqueSocketDescriptor &b) {
		return SocketDescriptor::CreateSocketPairNonBlock(domain, type,
								  protocol,
								  a, b);
	}
};

#endif
