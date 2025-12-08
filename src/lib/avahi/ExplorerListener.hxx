// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <string>

union InetAddress;
typedef struct AvahiStringList AvahiStringList;

namespace Avahi {

class ServiceExplorerListener {
public:
	struct Flags {
		/**
		 * This record/service resides on and was announced by
		 * the local host.
		 */
		bool is_local;

		/**
		 * This service belongs to the same local client as
		 * the browser object.
		 */
		bool is_our_own;
	};

	virtual void OnAvahiNewObject(const std::string &key,
				      const InetAddress &address,
				      AvahiStringList *txt,
				      Flags flags) noexcept = 0;
	virtual void OnAvahiRemoveObject(const std::string &key) noexcept = 0;
	virtual void OnAvahiAllForNow() noexcept {}
};

} // namespace Avahi
