/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Resolver.hxx"
#include "AddressInfo.hxx"
#include "HostParser.hxx"
#include "util/RuntimeError.hxx"

#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

static inline bool
ai_is_passive(const struct addrinfo *ai)
{
	return ai == nullptr || (ai->ai_flags & AI_PASSIVE) != 0;
}

static int
Resolve(const char *host_and_port, int default_port,
	const struct addrinfo *hints,
	struct addrinfo **ai_r)
{
	const char *host, *port;
	char buffer[256], port_string[16];

	if (host_and_port != nullptr) {
		const auto eh = ExtractHost(host_and_port);
		if (eh.HasFailed())
			return EAI_NONAME;

		if (eh.host.size >= sizeof(buffer)) {
			errno = ENAMETOOLONG;
			return EAI_SYSTEM;
		}

		port = eh.end;
		if (*port == ':') {
			/* port specified */
			++port;
			memcpy(buffer, eh.host.data, eh.host.size);
			buffer[eh.host.size] = 0;
			host = buffer;
		} else {
			/* no port specified */
			host = host_and_port;
			snprintf(port_string, sizeof(port_string), "%d", default_port);
			port = port_string;
		}

		if (ai_is_passive(hints) && strcmp(host, "*") == 0)
			host = nullptr;
	} else {
		host = nullptr;
		snprintf(port_string, sizeof(port_string), "%d", default_port);
		port = port_string;
	}

	return getaddrinfo(host, port, hints, ai_r);
}

AddressInfoList
Resolve(const char *host_and_port, int default_port,
	const struct addrinfo *hints)
{
	struct addrinfo *ai;
	int result = Resolve(host_and_port, default_port, hints, &ai);
	if (result != 0)
		throw FormatRuntimeError("Failed to resolve '%s': %s",
					 host_and_port, gai_strerror(result));

	return AddressInfoList(ai);
}
