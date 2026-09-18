// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Arch.hxx"
#include "TxtUtil.hxx"
#include "system/Arch.hxx"
#include "util/StringSplit.hxx"

using std::string_view_literals::operator""sv;

namespace Avahi {

Arch
GetArchFromTxt(AvahiStringList *txt) noexcept
{
	txt = avahi_string_list_find(txt, "arch");
	return txt != nullptr
		? ParseArch(GetValueFromTxt(*txt))
		: Arch::NONE;
}

} // namespace Avahi
