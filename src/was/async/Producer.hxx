// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

namespace Was {

/**
 * A producer for the #Output class.  It writes data to the pipe and
 * may get notified when the pipe becomes writable.
 */
class OutputProducer {
public:
	virtual void OnWasOutputReady() = 0;
};

} // namespace Was
