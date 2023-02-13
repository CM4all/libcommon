// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <boost/json.hpp>

namespace Json {

[[gnu::const]]
static inline const std::error_category &
ErrorCategory() noexcept
{
	/* the error_category is a local static variable in
	   make_error_code(), so we have no choice but to call this
	   function in order to obtain the error_category reference*/
	return boost::json::make_error_code(boost::json::error::syntax).category();
}

[[gnu::pure]]
static bool
IsJsonError(const boost::json::system_error &e) noexcept
{
	return e.code().category() == ErrorCategory();
}

} // namespace Json
