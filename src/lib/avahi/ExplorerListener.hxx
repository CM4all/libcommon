// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <string>

class SocketAddress;
typedef struct AvahiStringList AvahiStringList;

namespace Avahi {

class ServiceExplorerListener {
public:
	virtual void OnAvahiNewObject(const std::string &key,
				      SocketAddress address,
				      AvahiStringList *txt) noexcept = 0;
	virtual void OnAvahiRemoveObject(const std::string &key) noexcept = 0;
	virtual void OnAvahiAllForNow() noexcept {}
};

} // namespace Avahi
