/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "UdpListenerConfig.hxx"
#include "UniqueSocketDescriptor.hxx"
#include "ToString.hxx"
#include "system/Error.hxx"

#include <sys/un.h>
#include <unistd.h>

UniqueSocketDescriptor
UdpListenerConfig::Create()
{
	UniqueSocketDescriptor fd;
	if (!fd.CreateNonBlock(bind_address.GetFamily(), SOCK_DGRAM, 0))
		throw MakeErrno("Failed to create socket");

	if (bind_address.GetFamily() == AF_LOCAL) {
		const struct sockaddr_un *sun = (const struct sockaddr_un *)
			bind_address.GetAddress();
		if (sun->sun_path[0] != '\0')
			/* delete non-abstract socket files before reusing them */
			unlink(sun->sun_path);

		if (pass_cred)
			/* we want to receive the client's UID */
			fd.SetBoolOption(SOL_SOCKET, SO_PASSCRED, true);
	}

	/* set SO_REUSEADDR if we're using multicast; this option allows
	   multiple processes to join the same group on the same port */
	if (!multicast_group.IsNull() && !fd.SetReuseAddress(true))
		throw MakeErrno("Failed to set SO_REUSEADDR");

	if (!fd.Bind(bind_address)) {
		const int e = errno;

		char buffer[256];
		const char *address_string =
			ToString(buffer, sizeof(buffer), bind_address)
			? buffer
			: "?";

		throw FormatErrno(e, "Failed to bind to %s", address_string);
	}

	if (!multicast_group.IsNull() &&
	    !fd.AddMembership(multicast_group)) {
		const int e = errno;

		char buffer[256];
		const char *address_string =
			ToString(buffer, sizeof(buffer), multicast_group)
			? buffer
			: "?";

		throw FormatErrno(e, "Failed to join multicast group %s",
				  address_string);
	}

	return fd;
}
