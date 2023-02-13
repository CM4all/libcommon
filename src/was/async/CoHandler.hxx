// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "SimpleHandler.hxx"

namespace Co { template <typename T> class Task; }

namespace Was {

class CoSimpleRequestHandler : public SimpleRequestHandler {
	class Request;

public:
	bool OnRequest(SimpleServer &server,
		       SimpleRequest &&request,
		       CancellablePointer &cancel_ptr) noexcept final;

protected:
	virtual Co::Task<SimpleResponse> OnCoRequest(SimpleRequest request) = 0;
};

} // namespace Was
