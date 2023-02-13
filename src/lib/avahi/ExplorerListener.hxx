// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <string>

class SocketAddress;

namespace Avahi {

class ServiceExplorerListener {
public:
	virtual void OnAvahiNewObject(const std::string &key,
				      SocketAddress address) noexcept = 0;
	virtual void OnAvahiRemoveObject(const std::string &key) noexcept = 0;
};

} // namespace Avahi
