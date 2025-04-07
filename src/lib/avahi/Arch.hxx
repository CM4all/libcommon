// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <avahi-common/strlst.h>

class SocketAddress;

namespace Avahi {

/**
 * Add an "arch" TXT record according to the CPU architecture of the
 * running program.  This can be used by explorers to filter available
 * hosts by their CPU architecture.
 *
 * This function uses Debian architecture names, i.e. "amd64" instead
 * of "x86_64" and "arm64" instead of "aarch64".
 */
inline AvahiStringList *
AddArchTxt(AvahiStringList *txt) noexcept
{
#ifdef __x86_64__
	txt = avahi_string_list_add_pair(txt, "arch", "amd64");
#elif defined(__aarch64__)
	txt = avahi_string_list_add_pair(txt, "arch", "arm64");
#endif
	/* more architectures to be added once we have them */
	return txt;
}

} // namespace Avahi
