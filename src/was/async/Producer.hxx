// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

namespace Was {

class Output;

/**
 * A producer for the #Output class.  It writes data to the pipe and
 * may get notified when the pipe becomes writable.
 */
class OutputProducer {
public:
	/**
	 * This object has been registered in an #Output instance and
	 * the first write has been deferred.  This makes the #Output
	 * instance known to the producer.
	 */
	virtual void OnWasOutputBegin(Output &output) noexcept = 0;

	virtual void OnWasOutputReady() = 0;
};

} // namespace Was
