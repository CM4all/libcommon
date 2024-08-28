// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "SimpleHandler.hxx"
#include "Control.hxx"
#include "SimpleInput.hxx"
#include "SimpleOutput.hxx"
#include "util/Cancellable.hxx"

#include <exception>

struct WasSocket;

namespace Was {

class SimpleClientHandler {
public:
	virtual void OnWasError(std::exception_ptr error) noexcept = 0;
	virtual void OnWasClosed() noexcept = 0;
};

class SimpleResponseHandler {
public:
	/**
	 * A response was received.
	 */
	virtual void OnWasResponse(SimpleResponse &&response) noexcept = 0;
	virtual void OnWasError(std::exception_ptr error) noexcept = 0;
};

/**
 * A "simple" WAS client connection.
 */
class SimpleClient final
	: ControlHandler, SimpleInputHandler, SimpleOutputHandler, Cancellable
{
	Control control;
	SimpleInput input;
	SimpleOutput output;

	SimpleClientHandler &handler;

	SimpleResponse response;

	SimpleResponseHandler *response_handler = nullptr;

	enum class State {
		IDLE,
		HEADERS,
		BODY,
	} state = State::IDLE;

	bool stopping = false;

public:
	SimpleClient(EventLoop &event_loop, WasSocket &&socket,
		     SimpleClientHandler &_handler) noexcept;

	auto &GetEventLoop() const noexcept {
		return control.GetEventLoop();
	}

	void Close() noexcept {
		control.Close();
		input.Close();
		output.Close();
	}

	bool IsStopping() const noexcept {
		return stopping;
	}

	bool SendRequest(SimpleRequest &&request,
			 SimpleResponseHandler &_response_handler,
			 CancellablePointer &cancel_ptr) noexcept;

private:
	void Closed() noexcept;

	/**
	 * Abort receiving the response status/headers from the WAS server.
	 */
	void AbortError(std::exception_ptr error) noexcept;

	void AbortProtocolError(const char *msg) noexcept;

	/* virtual methods from class Was::ControlHandler */
	bool OnWasControlPacket(enum was_command cmd,
				std::span<const std::byte> payload) noexcept override;
	bool OnWasControlDrained() noexcept override;
	void OnWasControlDone() noexcept override;
	void OnWasControlHangup() noexcept override;
	void OnWasControlError(std::exception_ptr ep) noexcept override;

	/* virtual methods from class Was::SimpleInputHandler */
	void OnWasInput(DisposableBuffer input) noexcept override;
	void OnWasInputHangup() noexcept override;
	void OnWasInputError(std::exception_ptr error) noexcept override;

	/* virtual methods from class Was::SimpleOutputHandler */
	void OnWasOutputError(std::exception_ptr error) noexcept override;

	/* virtual methods from class Cancellable */
	void Cancel() noexcept override;
};

} // namespace Was
