// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <boost/json/fwd.hpp>

namespace Was {

struct SimpleRequest;
struct SimpleResponse;

/**
 * Parse a JSON request body.  Throws #BadRequest if the request body
 * is not JSON (according to the Content-Type header) and on JSON
 * parser errors.
 */
boost::json::value
ParseJson(const SimpleRequest &request);

/**
 * Serialize the given JSON value and place it as the response body
 * and add header "Content-Type: application/json".
 */
void
WriteJson(SimpleResponse &response, const boost::json::value &j) noexcept;

[[gnu::pure]]
SimpleResponse
ToResponse(const boost::json::value &j) noexcept;

} // namespace Was
