// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

class EventLoop;

namespace Was {

class SimpleRequestHandler;

/**
 * Accept incoming WAS requests using the given #EventLoop and let the
 * given #SimpleRequestHandler handle them.
 *
 * This function auto-detects how this process was launched:
 *
 * - classic WAS (single WAS connection on fds 0,1,3)
 * - Multi-WAS (socket on fd=0)
 * - systemd socket activation (listener socket on fd 3)
 *
 * This function does not return until the client closes the initial
 * connection.  Additionally, it installs handlers for SIGTERM, SIGINT
 * and SIGQUIT to initiate shutdown.
 *
 * @param event_loop the #EventLoop which is used to dispatch all
 * events
 * @param request_handler this class handles all requests
 * asynchronously
 */
void
Run(EventLoop &event_loop, SimpleRequestHandler &request_handler);

} // namespace Was
