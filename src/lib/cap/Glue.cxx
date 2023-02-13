// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Glue.hxx"
#include "State.hxx"

bool
IsSysAdmin() noexcept
try {
	const auto s = CapabilityState::Current();
	return s.GetFlag(CAP_SYS_ADMIN, CAP_EFFECTIVE) == CAP_SET;
} catch (...) {
	/* shouldn't happen, but if it does, pretend we're not
	   admin */
	return false;
}

bool
HaveSetuid() noexcept
try {
	const auto s = CapabilityState::Current();
	return s.GetFlag(CAP_SETUID, CAP_EFFECTIVE) == CAP_SET;
} catch (...) {
	/* shouldn't happen */
	return false;
}

bool
HaveNetBindService() noexcept
try {
	const auto s = CapabilityState::Current();
	return s.GetFlag(CAP_NET_BIND_SERVICE, CAP_EFFECTIVE) == CAP_SET;
} catch (...) {
	/* shouldn't happen */
	return false;
}
