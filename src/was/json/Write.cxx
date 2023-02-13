// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Write.hxx"
#include "was/WasOutputStream.hxx"
#include "json/Serialize.hxx"

extern "C" {
#include <was/simple.h>
}

#include <boost/json.hpp>

bool
WriteJsonResponse(struct was_simple *w,
		  const boost::json::value &j) noexcept
{
	if (!was_simple_set_header(w, "content-type", "application/json"))
		return false;

	try {
		WasOutputStream wos(w);
		Json::Serialize(wos, j);
		return true;
	} catch (WasOutputStream::WriteFailed) {
		return false;
	}
}
