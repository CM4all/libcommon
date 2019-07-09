/*
 * Copyright 2007-2019 Content Management AG
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

#include "JailParams.hxx"
#include "Prepared.hxx"
#include "AllocatorPtr.hxx"
#include "util/StaticArray.hxx"

#if TRANSLATION_ENABLE_EXPAND
#include "pexpand.hxx"
#endif

#include <stdexcept>

#include <string.h>

void
JailParams::Check() const
{
	if (!enabled)
		return;

	if (home_directory == nullptr)
		throw std::runtime_error("No JailCGI home directory");
}

JailParams::JailParams(AllocatorPtr alloc, const JailParams &src)
	:enabled(src.enabled),
#if TRANSLATION_ENABLE_EXPAND
	 expand_home_directory(src.expand_home_directory),
#endif
	 account_id(alloc.CheckDup(src.account_id)),
	 site_id(alloc.CheckDup(src.site_id)),
	 user_name(alloc.CheckDup(src.user_name)),
	 host_name(alloc.CheckDup(src.host_name)),
	 home_directory(alloc.CheckDup(src.home_directory))
{
}

char *
JailParams::MakeId(char *p) const
{
	if (enabled) {
		p = (char *)mempcpy(p, ";j=", 3);
		p = stpcpy(p, home_directory);
	}

	return p;
}

void
JailParams::InsertWrapper(PreparedChildProcess &p,
			  const char *document_root) const
{
	if (!enabled)
		return;

	StaticArray<const char *, 16> w;

	w.push_back("/usr/lib/cm4all/jailcgi/bin/wrapper");

	if (document_root != nullptr) {
		w.push_back("-d");
		w.push_back(document_root);
	}

	if (account_id != nullptr) {
		w.push_back("--account");
		w.push_back(account_id);
	}

	if (site_id != nullptr) {
		w.push_back("--site");
		w.push_back(site_id);
	}

	if (user_name != nullptr) {
		w.push_back("--name");
		w.push_back(user_name);
	}

	if (host_name != nullptr)
		p.SetEnv("JAILCGI_SERVERNAME", host_name);

	if (home_directory != nullptr) {
		w.push_back("--home");
		w.push_back(home_directory);
	}

	w.push_back("--");

	p.InsertWrapper({w.raw(), w.size()});
}

#if TRANSLATION_ENABLE_EXPAND

void
JailParams::Expand(AllocatorPtr alloc, const MatchInfo &match_info)
{
	if (expand_home_directory)
		home_directory =
			expand_string_unescaped(alloc, home_directory, match_info);
}

#endif
