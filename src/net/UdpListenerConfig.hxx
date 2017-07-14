/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef UDP_LISTENER_CONFIG_HXX
#define UDP_LISTENER_CONFIG_HXX

#include "AllocatedSocketAddress.hxx"

class UniqueSocketDescriptor;

struct UdpListenerConfig {
	AllocatedSocketAddress bind_address;

	AllocatedSocketAddress multicast_group;

	bool pass_cred = false;

	/**
	 * Create a listening socket.
	 *
	 * Throws exception on error.
	 */
	UniqueSocketDescriptor Create();
};

#endif
