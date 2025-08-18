// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "ServiceConfig.hxx"
#include "Service.hxx"
#include "Arch.hxx"
#include "Check.hxx"
#include "lib/fmt/ToBuffer.hxx"
#include "net/SocketAddress.hxx"
#include "io/config/FileLineParser.hxx"
#include "util/StringAPI.hxx"

#include <cassert>

#include <stdlib.h> // for strtod()

using std::string_view_literals::operator""sv;

namespace Avahi {

bool
ServiceConfig::ParseLine(const char *word, FileLineParser &line)
{
	if (StringIsEqual(word, "zeroconf_service")) {
		if (!service.empty())
			throw LineParser::Error("Duplicate Zeroconf service");

		service = MakeZeroconfServiceType(line.ExpectValueAndEnd(), "_tcp");
		return true;
	} else if (StringIsEqual(word, "zeroconf_domain")) {
		if (!domain.empty())
			throw LineParser::Error("Duplicate Zeroconf domain");

		domain = line.ExpectValueAndEnd();
		return true;
	} else if (StringIsEqual(word, "zeroconf_interface")) {
		if (service.empty())
			throw LineParser::Error("Zeroconf interface without service");

		if (!interface.empty())
			throw LineParser::Error("Duplicate Zeroconf interface");

		interface = line.ExpectValueAndEnd();
		return true;
	} else if (StringIsEqual(word, "zeroconf_weight")) {
		if (service.empty())
			throw LineParser::Error("zeroconf_weight without zeroconf_service");

		if (weight >= 0)
			throw LineParser::Error("Duplicate zeroconf_weight");

		const char *s = line.ExpectValueAndEnd();

		char *endptr;
		weight = strtod(s, &endptr);
		if (endptr == s || *endptr != '\0')
			throw LineParser::Error("Failed to parse number");

		if (weight <= 0 || weight > 1e6f)
			throw LineParser::Error("Bad zeroconf_weight value");

		return true;
	} else if (StringIsEqual(word, "zeroconf_protocol")) {
		if (service.empty())
			throw LineParser::Error("Zeroconf protocol without service");

		if (protocol != AVAHI_PROTO_UNSPEC)
			throw LineParser::Error("Duplicate Zeroconf protocol");

		const char *value = line.ExpectValueAndEnd();
		if (StringIsEqual(value, "inet"))
			protocol = AVAHI_PROTO_INET;
		else if (StringIsEqual(value, "inet6"))
			protocol = AVAHI_PROTO_INET6;
		else
			throw LineParser::Error("Unrecognized Zeroconf protocol");

		return true;
	} else
		return false;
}

void
ServiceConfig::Check() const
{
	if (IsEnabled()) {
	} else {
		if (!domain.empty())
			throw LineParser::Error("Zeroconf service missing");
	}
}

std::unique_ptr<Avahi::Service>
ServiceConfig::Create(const char *interface2,
		      SocketAddress local_address,
		      bool v6only) const noexcept
{
	assert(IsEnabled());

	auto s = std::make_unique<Service>(service.c_str(),
					   !interface.empty()
					   ? interface.c_str()
					   : interface2,
					   local_address,
					   v6only);

	if (protocol != AVAHI_PROTO_UNSPEC)
		s->protocol = protocol;

	AvahiStringList *txt = nullptr;

	txt = Avahi::AddArchTxt(txt);

	if (weight >= 0)
		txt = avahi_string_list_add_pair(txt, "weight",
						 FmtBuffer<64>("{}"sv, weight));

	s->txt.reset(txt);
	return s;
}

} // namespace Avahi
