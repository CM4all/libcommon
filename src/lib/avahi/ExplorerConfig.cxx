// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "ExplorerConfig.hxx"
#include "Check.hxx"
#include "Explorer.hxx"
#include "lib/fmt/SystemError.hxx"
#include "io/config/FileLineParser.hxx"
#include "util/StringAPI.hxx"

#include <cassert>

#include <net/if.h>

namespace Avahi {

bool
ServiceExplorerConfig::ParseLine(const char *word, FileLineParser &line)
{
	if (StringIsEqual(word, "service")) {
		if (!service.empty())
			throw LineParser::Error("Duplicate Zeroconf service");

		service = MakeZeroconfServiceType(line.ExpectValueAndEnd(), "_tcp");
		return true;
	} else if (StringIsEqual(word, "domain")) {
		if (!domain.empty())
			throw LineParser::Error("Duplicate Zeroconf domain");

		domain = line.ExpectValueAndEnd();
		return true;
	} else if (StringIsEqual(word, "interface")) {
		if (service.empty())
			throw LineParser::Error("Zeroconf interface without service");

		if (!interface.empty())
			throw LineParser::Error("Duplicate Zeroconf interface");

		interface = line.ExpectValueAndEnd();
		return true;
	} else if (StringIsEqual(word, "protocol")) {
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
ServiceExplorerConfig::Check() const
{
	if (IsEnabled()) {
	} else {
		if (!domain.empty())
			throw LineParser::Error("Zeroconf service missing");
	}
}

std::unique_ptr<Avahi::ServiceExplorer>
ServiceExplorerConfig::Create(Avahi::Client &client,
				Avahi::ServiceExplorerListener &listener,
				Avahi::ErrorHandler &error_handler) const
{
	assert(IsEnabled());

	AvahiIfIndex interface_ = AVAHI_IF_UNSPEC;

	if (!interface.empty()) {
		int i = if_nametoindex(interface.c_str());
		if (i == 0)
			throw FmtErrno("Failed to find interface {:?}", interface);

		interface_ = static_cast<AvahiIfIndex>(i);
	}

	return std::make_unique<Avahi::ServiceExplorer>(client, listener,
							interface_,
							protocol,
							service.c_str(),
							domain.empty() ? nullptr : domain.c_str(),
							error_handler);
}

} // namespace Avahi
