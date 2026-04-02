// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Weight.hxx"
#include "StringListCast.hxx"

#include <stdlib.h> // for strtod()

using std::string_view_literals::operator""sv;

namespace Avahi {

[[gnu::pure]]
double
GetWeightFromTxt(AvahiStringList *txt) noexcept
{
	constexpr std::string_view prefix = "weight="sv;
	txt = avahi_string_list_find(txt, "weight");
	if (txt == nullptr)
		/* there's no "weight" record */
		return 1.0;

	const char *s = reinterpret_cast<const char *>(txt->text) + prefix.size();
	char *endptr;
	double value = strtod(s, &endptr);
	if (endptr == s || *endptr != '\0' || value <= 0 || value > 1e6)
		/* parser failed: fall back to default value */
		return 1.0;

	return value;
}

} // namespace Avahi
