// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "SimpleServer.hxx"
#include "Socket.hxx"
#include "net/SocketProtocolError.hxx"
#include "util/SpanCast.hxx"
#include "util/StringFormat.hxx"
#include "util/StringSplit.hxx"

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
	/* avoid sending uninitialized data when STOP is received
	   without ever receiving a request; that would be a protocol
	   violation, but we don't care enough to check this
	   explicitly */
	output.ResetPosition();
}

inline bool
SimpleServer::SubmitRequest() noexcept
{
	assert(request.state == Request::State::BODY ||
	       request.state == Request::State::PENDING);

	request.state = Request::State::SUBMITTED;

	return request_handler.OnRequest(*this,
					 std::move(*request.request),
					 request.cancel_ptr);
}

bool
SimpleServer::CancelRequest() noexcept
{
	request.state = Request::State::NONE;
	request.request.reset();

	if (!request.cancel_ptr)
		return false;

	request.cancel_ptr.Cancel();
	return true;
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
	AbortError(std::make_exception_ptr(SocketProtocolError{msg}));
}

/*
 * Control channel handler
 */

bool
SimpleServer::OnWasControlPacket(enum was_command cmd,
				 std::span<const std::byte> payload) noexcept
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
		request.method = request.request->method = HttpMethod::GET;
		request.state = Request::State::HEADERS;
		//response.body = nullptr;
		output.ResetPosition();
		break;

	case WAS_COMMAND_METHOD:
		if (request.state != Request::State::HEADERS) {
			AbortProtocolError("misplaced METHOD packet");
			return false;
		}

		if (payload.size() != sizeof(uint32_t)) {
			AbortProtocolError("malformed METHOD packet");
			return false;
		}

		{
			auto method = static_cast<HttpMethod>(*(const uint32_t *)(const void *)payload.data());
			if (request.request->method != HttpMethod::GET &&
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

		request.request->uri.assign((const char *)payload.data(),
					    payload.size());
		break;

	case WAS_COMMAND_SCRIPT_NAME:
		if (request.state != Request::State::HEADERS)
			AbortProtocolError("misplaced SCRIPT_NAME packet");

		request.request->script_name.assign((const char *)payload.data(),
						    payload.size());
		break;

	case WAS_COMMAND_PATH_INFO:
		if (request.state != Request::State::HEADERS)
			AbortProtocolError("misplaced PATH_INFO packet");

		request.request->path_info.assign((const char *)payload.data(),
						    payload.size());
		break;

	case WAS_COMMAND_QUERY_STRING:
		if (request.state != Request::State::HEADERS)
			AbortProtocolError("misplaced QUERY_STRING packet");

		request.request->query_string.assign((const char *)payload.data(),
						     payload.size());
		break;

	case WAS_COMMAND_HEADER:
		if (request.state != Request::State::HEADERS) {
			AbortProtocolError("misplaced HEADER packet");
			return false;
		}

		if (auto [name, value] = Split(ToStringView(payload), '=');
		    value.data() != nullptr) {
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

		if (auto [name, value] = Split(ToStringView(payload), '=');
		    value.data() != nullptr) {
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
			auto length_p = (const uint64_t *)(const void *)payload.data();
			if (payload.size() != sizeof(*length_p)) {
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
		if (CancelRequest())
			/* the handler was canceled before it could
			   produce a response */
			return control.SendUint64(WAS_COMMAND_PREMATURE, 0);

		return control.SendUint64(WAS_COMMAND_PREMATURE,
					  output.Stop());

	case WAS_COMMAND_PREMATURE:
		{
			auto length_p = (const uint64_t *)(const void *)payload.data();
			if (payload.size() != sizeof(*length_p)) {
				AbortProtocolError("malformed PREMATURE packet");
				return false;
			}

			try {
				input.Premature(*length_p);
			} catch (...) {
				AbortError(std::current_exception());
				return false;
			}
		}

		return false;

	case WAS_COMMAND_REMOTE_HOST:
		if (request.state != Request::State::HEADERS)
			AbortProtocolError("misplaced REMOTE_HOST packet");

		request.request->remote_host.assign((const char *)payload.data(),
						    payload.size());
		break;

	case WAS_COMMAND_METRIC:
		// TODO implemnet
		break;
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
		return SubmitRequest();
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

	request.request->body = std::move(body);
	SubmitRequest();
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

	request.state = Request::State::NONE;
	request.request.reset();

	request.cancel_ptr = nullptr;

	if (!control.SendT(WAS_COMMAND_STATUS, response.status))
		return false;

	if (response.body && http_method_is_empty(request.method)) {
		if (request.method == HttpMethod::HEAD)
			response.headers.emplace("content-length",
						 StringFormat<64>("%zu", response.body.size()).c_str());

		response.body = {};
	}

	for (const auto &i : response.headers)
		if (!control.SendPair(WAS_COMMAND_HEADER, i.first, i.second))
			return false;

	if (response.body) {
		if (!control.Send(WAS_COMMAND_DATA) ||
		    !control.SendUint64(WAS_COMMAND_LENGTH, response.body.size()))
			return false;

		output.Activate(std::move(response.body));
	} else {
		if (!control.Send(WAS_COMMAND_NO_DATA))
			return false;
	}

	return true;
}

} // namespace Was
