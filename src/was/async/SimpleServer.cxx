/*
 * Copyright 2007-2021 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "SimpleServer.hxx"
#include "Error.hxx"
#include "util/StringFormat.hxx"
#include "util/StringView.hxx"

#include <array>

namespace Was {

SimpleServer::SimpleServer(EventLoop &event_loop, WasSocket &&socket,
			   SimpleServerHandler &_handler,
			   SimpleRequestHandler &_request_handler) noexcept
	:control(event_loop, socket.control.Release(), *this),
	 input(event_loop, std::move(socket.input), *this),
	 output(event_loop, std::move(socket.output), *this),
	 handler(_handler), request_handler(_request_handler)
{
}

void
SimpleServer::CancelRequest() noexcept
{
	if (request.cancel_ptr)
		request.cancel_ptr.CancelAndClear();
}

void
SimpleServer::Closed() noexcept
{
	CancelRequest();
	handler.OnWasClosed(*this);
}

void
SimpleServer::AbortError(std::exception_ptr error) noexcept
{
	CancelRequest();
	handler.OnWasError(*this, error);
}

void
SimpleServer::AbortProtocolError(const char *msg) noexcept
{
	AbortError(std::make_exception_ptr(WasProtocolError(msg)));
}

/*
 * Control channel handler
 */

bool
SimpleServer::OnWasControlPacket(enum was_command cmd,
				 ConstBuffer<void> payload) noexcept
{
	switch (cmd) {
	case WAS_COMMAND_NOP:
		break;

	case WAS_COMMAND_REQUEST:
		if (request.state != Request::State::NONE ||
		    output.IsActive()) {
			AbortProtocolError("misplaced REQUEST packet");
			return false;
		}

		assert(!request.request);
		request.request.emplace();
		request.method = request.request->method = HTTP_METHOD_GET;
		request.state = Request::State::HEADERS;
		//response.body = nullptr;
		break;

	case WAS_COMMAND_METHOD:
		if (request.state != Request::State::HEADERS) {
			AbortProtocolError("misplaced METHOD packet");
			return false;
		}

		if (payload.size != sizeof(request.request->method)) {
			AbortProtocolError("malformed METHOD packet");
			return false;
		}

		{
			auto method = *(const http_method_t *)payload.data;
			if (request.request->method != HTTP_METHOD_GET &&
			    method != request.request->method) {
				/* sending that packet twice is illegal */
				AbortProtocolError("misplaced METHOD packet");
				return false;
			}

			if (!http_method_is_valid(method)) {
				AbortProtocolError("invalid METHOD packet");
				return false;
			}

			request.method = request.request->method = method;
		}

		break;

	case WAS_COMMAND_URI:
		if (request.state != Request::State::HEADERS ||
		    !request.request->uri.empty()) {
			AbortProtocolError("misplaced URI packet");
			return false;
		}

		request.request->uri = {(const char *)payload.data, payload.size};
		break;

	case WAS_COMMAND_SCRIPT_NAME:
	case WAS_COMMAND_PATH_INFO:
	case WAS_COMMAND_QUERY_STRING:
		// XXX
		break;

	case WAS_COMMAND_HEADER:
		if (request.state != Request::State::HEADERS) {
			AbortProtocolError("misplaced HEADER packet");
			return false;
		}

		if (auto [name, value] = StringView{payload}.Split('=');
		    value != nullptr) {
			request.request->headers.emplace(name, value);
		} else {
			AbortProtocolError("malformed HEADER packet");
			return false;
		}

		break;

	case WAS_COMMAND_PARAMETER:
		if (request.state != Request::State::HEADERS) {
			AbortProtocolError("misplaced PARAMETER packet");
			return false;
		}

		if (auto [name, value] = StringView{payload}.Split('=');
		    value != nullptr) {
			request.request->parameters.emplace(name, value);
		} else {
			AbortProtocolError("malformed PARAMETER packet");
			return false;
		}

		break;

	case WAS_COMMAND_STATUS:
		AbortProtocolError("misplaced STATUS packet");
		return false;

	case WAS_COMMAND_NO_DATA:
		if (request.state != Request::State::HEADERS ||
		    request.request->uri.empty()) {
			AbortProtocolError("misplaced NO_DATA packet");
			return false;
		}

		request.state = Request::State::PENDING;
		break;

	case WAS_COMMAND_DATA:
		if (request.state != Request::State::HEADERS ||
		    request.request->uri.empty()) {
			AbortProtocolError("misplaced DATA packet");
			return false;
		}

		input.Activate();
		request.state = Request::State::BODY;
		break;

	case WAS_COMMAND_LENGTH:
		if (request.state < Request::State::BODY ||
		    !input.IsActive()) {
			AbortProtocolError("misplaced LENGTH packet");
			return false;
		}

		{
			auto length_p = (const uint64_t *)payload.data;
			if (payload.size != sizeof(*length_p)) {
				AbortProtocolError("malformed LENGTH packet");
				return false;
			}

			if (!input.SetLength(*length_p)) {
				AbortProtocolError("invalid LENGTH packet");
				return false;
			}
		}

		break;

	case WAS_COMMAND_STOP:
		// XXX
		AbortProtocolError(StringFormat<64>("unexpected packet: %d", cmd));
		return false;

	case WAS_COMMAND_PREMATURE:
		{
			auto length_p = (const uint64_t *)payload.data;
			if (payload.size != sizeof(*length_p)) {
				AbortError(std::make_exception_ptr("malformed PREMATURE packet"));
				return false;
			}

			if (!input.IsActive())
				break;

			input.Premature(*length_p);
		}

		return false;
	}

	return true;
}

bool
SimpleServer::OnWasControlDrained() noexcept
{
	if (request.state == Request::State::BODY) {
		request.request->body = input.CheckComplete();
		if (request.request->body)
			request.state = Request::State::PENDING;
	}

	if (request.state == Request::State::PENDING) {
		request.state = Request::State::SUBMITTED;

		return request_handler.OnRequest(*this,
						 std::move(*request.request),
						 request.cancel_ptr);
	}

	return true;
}

void
SimpleServer::OnWasControlDone() noexcept
{
	assert(!control.IsDefined());
}

void
SimpleServer::OnWasControlError(std::exception_ptr error) noexcept
{
	assert(!control.IsDefined());

	AbortError(error);
}

void
SimpleServer::OnWasInput(DisposableBuffer body) noexcept
{
	assert(request.state == Request::State::BODY);

	request.state = Request::State::SUBMITTED;

	request.request->body = std::move(body);
	request_handler.OnRequest(*this, std::move(*request.request),
				  request.cancel_ptr);

}

void
SimpleServer::OnWasInputError(std::exception_ptr error) noexcept
{
	AbortError(error);
}

void
SimpleServer::OnWasOutputError(std::exception_ptr error) noexcept
{
	AbortError(error);
}

bool
SimpleServer::SendResponse(SimpleResponse &&response) noexcept
{
	assert(request.state == Request::State::SUBMITTED);
	assert(request.request);
	//assert(response.body == nullptr);
	//assert(http_status_is_valid(response.status));
	assert(!http_status_is_empty(response.status) || !response.body);

	control.BulkOn();
	request.state = Request::State::NONE;
	request.request.reset();

	request.cancel_ptr = nullptr;

	if (!control.Send(WAS_COMMAND_STATUS, &response.status,
			  sizeof(response.status)))
		return false;

	if (response.body && http_method_is_empty(request.method)) {
		if (request.method == HTTP_METHOD_HEAD)
			response.headers.emplace("content-length",
						 StringFormat<64>("%zu", response.body.size()).c_str());

		response.body = {};
	}

	for (const auto &i : response.headers)
		control.SendPair(WAS_COMMAND_HEADER, i.first, i.second);

	if (response.body) {
		if (!control.SendEmpty(WAS_COMMAND_DATA) ||
		    !control.SendUint64(WAS_COMMAND_LENGTH, response.body.size()))
			return false;

		output.Activate(std::move(response.body));
	} else {
		if (!control.SendEmpty(WAS_COMMAND_NO_DATA))
			return false;
	}

	return control.BulkOff();
}

} // namespace Was
