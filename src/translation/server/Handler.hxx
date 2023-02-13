// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

class CancellablePointer;

namespace Translation::Server {

struct Request;
class Connection;

class Handler {
public:
	/**
	 * @return false if the #Connection has been destroyed
	 */
	virtual bool OnTranslationRequest(Connection &connection,
					  const Request &request,
					  CancellablePointer &cancel_ptr) noexcept = 0;
};

} // namespace Translation::Server
