// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "ExceptionResponse.hxx"
#include "SimpleResponse.hxx"

extern "C" {
#include <was/simple.h>
}

/**
 * Create a new #was_simple object and call the given function for
 * each incoming request.
 *
 * The given function may throw #Was::NotFound, #Was::BadRequest and
 * this loop will send the according response.
 */
template<typename F>
void
WasLoop(F &&f) noexcept
{
	auto *was = was_simple_new();
	const char *uri;
	while ((uri = was_simple_accept(was)) != nullptr) {
		try {
			f(was, uri);
		} catch (const Was::EndResponse &) {
			was_simple_end(was);
		} catch (const Was::AbortResponse &) {
			was_simple_abort(was);
		} catch (const Was::NotFound &e) {
			Was::SendNotFound(was, e.body);
		} catch (const Was::BadRequest &e) {
			Was::SendBadRequest(was, e.body);
		}
	}

	was_simple_free(was);
}
