// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "FunctionHandler.hxx"
#include "Response.hxx"
#include "Connection.hxx"

namespace Translation::Server {

bool
FunctionHandler::OnTranslationRequest(Connection &connection,
				      const Request &request,
				      CancellablePointer &) noexcept
{
	return connection.SendResponse(function(request));
}

} // namespace Translation::Server
