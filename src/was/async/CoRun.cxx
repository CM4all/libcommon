// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "CoRun.hxx"
#include "CoHandler.hxx"
#include "SimpleRun.hxx"

#include <functional>

namespace Was {

class CoRunAdapter final : public CoSimpleRequestHandler {
	const CoCallback handler;

public:
	explicit CoRunAdapter(CoCallback &&_handler) noexcept
		:handler(std::move(_handler)) {}

protected:
	Co::Task<SimpleResponse> OnCoRequest(SimpleRequest request) override {
		return handler(std::move(request));
	}
};

void
Run(EventLoop &event_loop, CoCallback handler)
{
	CoRunAdapter adapter{std::move(handler)};
	Run(event_loop, adapter);
}

} // namespace Was
