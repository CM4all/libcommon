// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <avahi-common/strlst.h>

namespace Avahi {

[[gnu::pure]]
double
GetWeightFromTxt(AvahiStringList *txt) noexcept;

} // namespace Avahi
