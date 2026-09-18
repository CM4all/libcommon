// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "TxtUtil.hxx"
#include "StringListCast.hxx"
#include "util/StringSplit.hxx"

namespace Avahi {

std::string_view
GetValueFromTxt(const AvahiStringList &src) noexcept
{
	return Split(ToStringView(src), '=').second;
}

} // namespace Avahi
