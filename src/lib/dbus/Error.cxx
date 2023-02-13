// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Error.hxx"
#include "util/RuntimeError.hxx"

void
ODBus::Error::Throw(const char *prefix) const
{
	throw FormatRuntimeError("%s: %s", prefix, GetMessage());
}

void
ODBus::Error::CheckThrow(const char *prefix) const
{
	if (*this)
		Throw(prefix);
}
