// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Handler.hxx"

namespace Co { template <typename T> class Task; }

namespace Translation::Server {

class Response;

/**
 * A variant of #Handler where the virtual method to be implemented is
 * a coroutine.
 */
class CoHandler : public Handler {
public:
	virtual Co::Task<Response> OnTranslationRequest(const Request &request) noexcept = 0;

	/* virtual methods from class Translation::Server::Handler */
	bool OnTranslationRequest(Connection &connection,
				  const Request &request,
				  CancellablePointer &cancel_ptr) noexcept final;
};

} // namespace Translation::Server
