// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Arch.hxx"
#include "StringListCast.hxx"
#include "system/Arch.hxx"

using std::string_view_literals::operator""sv;

namespace Avahi {

Arch
GetArchFromTxt(AvahiStringList *txt) noexcept
{
	constexpr std::string_view prefix = "arch="sv;
	txt = avahi_string_list_find(txt, "arch");
	return txt != nullptr
		? ParseArch(Avahi::ToStringView(*txt).substr(prefix.size()))
		: Arch::NONE;
}

} // namespace Avahi
