// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "SocketConfig.hxx"
#include "Parser.hxx"
#include "io/config/LineParser.hxx"
#include "util/StringAPI.hxx"

bool
SocketConfig::ParseLine(const char *word, LineParser &line,
			int type, unsigned default_port)
{
	if (StringIsEqual(word, "bind")) {
		if (!bind_address.IsNull())
			throw LineParser::Error{"Bind address already specified"};

		bind_address = ParseSocketAddress(line.ExpectValueAndEnd(), default_port, true);
		return true;
#ifdef HAVE_TCP
	} else if (type == SOCK_STREAM && StringIsEqual(word, "ack_timeout")) {
		tcp_user_timeout = line.NextPositiveInteger() * 1000;
		line.ExpectEnd();
		return true;
	} else if ((type == SOCK_STREAM || type == SOCK_SEQPACKET) && StringIsEqual(word, "keepalive")) {
		keepalive = line.NextBool();
		line.ExpectEnd();
		return true;
	} else if (StringIsEqual(word, "reuse_port")) {
		reuse_port = line.NextBool();
		line.ExpectEnd();
		return true;
#ifdef __linux__
	} else if (StringIsEqual(word, "interface")) {
		interface = line.ExpectValueAndEnd();
		return true;
	} else if (type == SOCK_STREAM && StringIsEqual(word, "mptcp")) {
		mptcp = line.NextBool();
		line.ExpectEnd();
		return true;
	} else if (StringIsEqual(word, "free_bind")) {
		free_bind = line.NextBool();
		line.ExpectEnd();
		return true;
#endif // __linux__
#endif // HAVE_TCP
#ifdef HAVE_UN
	} else if (StringIsEqual(word, "mode")) {
		if (bind_address.IsNull() ||
		    bind_address.GetFamily() != AF_LOCAL)
			throw LineParser::Error("'mode' works only with local sockets");

		const char *s = line.ExpectValueAndEnd();
		char *endptr;
		const unsigned long value = strtoul(s, &endptr, 8);
		if (endptr == s || *endptr != 0)
			throw LineParser::Error("Not a valid octal value");

		if (value & ~0777ULL)
			throw LineParser::Error("Not a valid mode");

		mode = value;
		return true;
#endif // HAVE_UN
#ifdef HAVE_IPV6
	} else if (StringIsEqual(word, "v6only")) {
		v6only = line.NextBool();
		line.ExpectEnd();
		return true;
#endif
	} else
		return false;
}
