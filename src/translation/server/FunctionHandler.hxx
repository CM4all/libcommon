// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Handler.hxx"

#include <functional>

namespace Translation::Server {

class Response;

using HandlerFunction = std::function<Response(const Request &)>;

/**
 * An implementation of #Handler which calls a std::function.
 */
class FunctionHandler : public Handler {
	HandlerFunction function;

public:
	template<typename F>
	explicit FunctionHandler(F &&_function) noexcept
		:function(std::forward<F>(_function)) {}

	/* virtual methods from class Translation::Server::Handler */
	bool OnTranslationRequest(Connection &connection,
				  const Request &request,
				  CancellablePointer &cancel_ptr) noexcept final;
};

} // namespace Translation::Server
