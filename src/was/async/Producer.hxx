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
	virtual ~OutputProducer() noexcept = default;

	/**
	 * This object has been registered in an #Output instance and
	 * the first write has been deferred.  This makes the #Output
	 * instance known to the producer.
	 *
	 * @return false if this object has been destroyed
	 */
	[[nodiscard]]
	virtual bool OnWasOutputBegin(Output &output) noexcept = 0;

	virtual void OnWasOutputReady() = 0;
};

} // namespace Was
