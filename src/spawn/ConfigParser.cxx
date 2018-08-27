/*
 * Copyright 2007-2018 Content Management AG
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ConfigParser.hxx"
#include "Config.hxx"
#include "io/FileLineParser.hxx"
#include "util/RuntimeError.hxx"

#include <pwd.h>
#include <grp.h>

static uid_t
ParseUser(const char *name)
{
	char *endptr;
	unsigned long i = strtoul(name, &endptr, 10);
	if (endptr > name && *endptr == 0)
		return i;

	const auto *pw = getpwnam(name);
	if (pw == nullptr)
		throw FormatRuntimeError("No such user: %s", name);

	return pw->pw_uid;
}

static gid_t
ParseGroup(const char *name)
{
	char *endptr;
	unsigned long i = strtoul(name, &endptr, 10);
	if (endptr > name && *endptr == 0)
		return i;

	const auto *gr = getgrnam(name);
	if (gr == nullptr)
		throw FormatRuntimeError("No such group: %s", name);

	return gr->gr_gid;
}

void
SpawnConfigParser::ParseLine(FileLineParser &line)
{
	const char *word = line.ExpectWord();

	if (strcmp(word, "allow_user") == 0) {
		config.allowed_uids.insert(ParseUser(line.ExpectValueAndEnd()));
	} else if (strcmp(word, "allow_group") == 0) {
		config.allowed_gids.insert(ParseGroup(line.ExpectValueAndEnd()));
	} else
		throw LineParser::Error("Unknown option");
}
