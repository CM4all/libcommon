// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "SimpleHandler.hxx"
#include "Control.hxx"
#include "Socket.hxx"
#include "SimpleInput.hxx"
#include "SimpleOutput.hxx"
#include "http/Method.hxx"
#include "util/Cancellable.hxx"
#include "util/DisposableBuffer.hxx"

#include <optional>

namespace Was {

class SimpleServerHandler {
public:
	virtual void OnWasError(SimpleServer &server,
				std::exception_ptr error) noexcept = 0;
	virtual void OnWasClosed(SimpleServer &server) noexcept = 0;
};

/**
 * A "simple" WAS server connection.
 */
class SimpleServer final
	: ControlHandler, SimpleInputHandler, SimpleOutputHandler
{
	Control control;
	SimpleInput input;
	SimpleOutput output;

	SimpleServerHandler &handler;
	SimpleRequestHandler &request_handler;

	struct Request {
		HttpMethod method = HttpMethod::GET;
		std::optional<SimpleRequest> request;

		CancellablePointer cancel_ptr{nullptr};

		enum class State : uint8_t {
			/**
			 * No request is being processed currently.
			 */
			NONE,

			/**
			 * Receiving headers.
			 */
			HEADERS,

			/**
			 * Reading the request body.
			 */
			BODY,

			/**
			 * Pending call to
			 * SimpleServerHandler::OnRequest().
			 */
			PENDING,

			/**
			 * Request already submitted to
			 * SimpleServerHandler::OnRequest().
			 */
			SUBMITTED,
		} state = State::NONE;
	} request;

public:
	SimpleServer(EventLoop &event_loop, WasSocket &&socket,
		     SimpleServerHandler &_handler,
		     SimpleRequestHandler &_request_handler) noexcept;

	auto &GetEventLoop() const noexcept {
		return control.GetEventLoop();
	}

	bool SendResponse(SimpleResponse &&response) noexcept;

private:
	bool SubmitRequest() noexcept;

	/**
	 * @return true if a request handler was canceled, false if
	 * there is no request currently
	 */
	bool CancelRequest() noexcept;

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
	void OnWasControlError(std::exception_ptr ep) noexcept override;

	/* virtual methods from class Was::SimpleInputHandler */
	void OnWasInput(DisposableBuffer input) noexcept override;
	void OnWasInputError(std::exception_ptr error) noexcept override;

	/* virtual methods from class Was::SimpleOutputHandler */
	void OnWasOutputError(std::exception_ptr error) noexcept override;
};

} // namespace Was
