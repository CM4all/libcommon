// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "SimpleClient.hxx"
#include "Socket.hxx"
#include "net/SocketProtocolError.hxx"
#include "util/SpanCast.hxx"
#include "util/StringSplit.hxx"
#include "util/Unaligned.hxx"

#include <array>

namespace Was {

SimpleClient::SimpleClient(EventLoop &event_loop, WasSocket &&socket,
			   SimpleClientHandler &_handler) noexcept
	:control(event_loop, std::move(socket.control), *this),
	 input(event_loop, std::move(socket.input), *this),
	 output(event_loop, std::move(socket.output), *this),
	 handler(_handler)
{
	/* avoid sending uninitialized data when STOP is received
	   without ever receiving a request; that would be a protocol
	   violation, but we don't care enough to check this
	   explicitly */
	output.ResetPosition();
}

static bool
SendMap(Control &control, enum was_command cmd, const auto &map) noexcept
{
	for (const auto &[key, value] : map)
		if (!control.SendPair(cmd, key, value))
			return false;

	return true;
}

static bool
SendRequest(Control &control, const SimpleRequest &request) noexcept
{
	return control.Send(WAS_COMMAND_REQUEST) &&
	       (request.method == HttpMethod::GET ||
		control.SendT(WAS_COMMAND_METHOD, static_cast<uint32_t>(request.method))) &&
	       control.SendString(WAS_COMMAND_URI, request.uri) &&
	       (request.script_name.empty() ||
		control.SendString(WAS_COMMAND_SCRIPT_NAME, request.script_name)) &&
	       (request.path_info.empty() ||
		control.SendString(WAS_COMMAND_PATH_INFO, request.path_info)) &&
	       (request.query_string.empty() ||
		control.SendString(WAS_COMMAND_QUERY_STRING, request.query_string)) &&
	       SendMap(control, WAS_COMMAND_HEADER, request.headers) &&
	       SendMap(control, WAS_COMMAND_PARAMETER, request.parameters) &&
	       (request.remote_host.empty() ||
		control.SendString(WAS_COMMAND_REMOTE_HOST, request.remote_host)) &&
	       control.Send(request.body
			    ? WAS_COMMAND_DATA
			    : WAS_COMMAND_NO_DATA) &&
	       (!request.body || control.SendUint64(WAS_COMMAND_LENGTH, request.body.size()));
}

bool
SimpleClient::SendRequest(SimpleRequest &&request,
			  SimpleResponseHandler &_response_handler,
			  CancellablePointer &cancel_ptr) noexcept
{
	assert(state == State::IDLE);

	cancel_ptr = *this;
	response_handler = &_response_handler;
	state = State::HEADERS;
	response = {};

	if (!Was::SendRequest(control, request))
		return false;

	if (request.body)
		output.Activate(std::move(request.body));

	return true;
}

void
SimpleClient::Closed() noexcept
{
	if (state != State::IDLE)
		response_handler->OnWasError(std::make_exception_ptr(SocketClosedPrematurelyError{}));

	Close();
	handler.OnWasClosed();
}

void
SimpleClient::AbortError(std::exception_ptr error) noexcept
{
	if (state != State::IDLE)
		response_handler->OnWasError(error);

	Close();
	handler.OnWasError(error);
}

void
SimpleClient::AbortProtocolError(const char *msg) noexcept
{
	AbortError(std::make_exception_ptr(SocketProtocolError{msg}));
}

bool
SimpleClient::OnWasControlPacket(enum was_command cmd,
				 std::span<const std::byte> payload) noexcept
{
	switch (cmd) {
	case WAS_COMMAND_NOP:
		break;

	case WAS_COMMAND_REQUEST:
	case WAS_COMMAND_METHOD:
	case WAS_COMMAND_URI:
	case WAS_COMMAND_SCRIPT_NAME:
	case WAS_COMMAND_PATH_INFO:
	case WAS_COMMAND_QUERY_STRING:
	case WAS_COMMAND_PARAMETER:
	case WAS_COMMAND_REMOTE_HOST:
		AbortProtocolError("misplaced request packet");
		return false;

	case WAS_COMMAND_HEADER:
		if (state != State::HEADERS) {
			AbortProtocolError("misplaced HEADER packet");
			return false;
		}

		if (auto [name, value] = Split(ToStringView(payload), '=');
		    value.data() != nullptr) {
			response.headers.emplace(name, value);
		} else {
			AbortProtocolError("malformed HEADER packet");
			return false;
		}

		break;

	case WAS_COMMAND_STATUS:
		if (state != State::HEADERS) {
			AbortProtocolError("misplaced STATUS packet");
			return false;
		}

		if (payload.size() != sizeof(response.status)) {
			AbortProtocolError("malformed STATUS packet");
			return false;
		}

		if (const auto status = LoadUnaligned<HttpStatus>(payload.data());
		    http_status_is_valid(status)) {
			response.status = status;
		} else {
			AbortProtocolError("invalid STATUS packet");
			return false;
		}

		break;

	case WAS_COMMAND_NO_DATA:
		if (state != State::HEADERS) {
			AbortProtocolError("misplaced NO_DATA packet");
			return false;
		}

		state = State::IDLE;
		response_handler->OnWasResponse(std::move(response));
		break;

	case WAS_COMMAND_DATA:
		if (state != State::HEADERS) {
			AbortProtocolError("misplaced DATA packet");
			return false;
		}

		state = State::BODY;
		input.Activate();
		break;

	case WAS_COMMAND_LENGTH:
		if (state != State::BODY) {
			AbortProtocolError("misplaced LENGTH packet");
			return false;
		}

		assert(input.IsActive());

		if (payload.size() != sizeof(uint64_t)) {
			AbortProtocolError("malformed LENGTH packet");
			return false;
		}

		if (!input.SetLength(LoadUnaligned<uint64_t>(payload.data()))) {
			AbortProtocolError("invalid LENGTH packet");
			return false;
		}

		break;

	case WAS_COMMAND_STOP:
		return control.SendUint64(WAS_COMMAND_PREMATURE,
					  output.Stop());

	case WAS_COMMAND_PREMATURE:
		if (state != State::BODY && !stopping) {
			AbortProtocolError("misplaced PREMATURE packet");
			return false;
		}

		if (payload.size() != sizeof(uint64_t)) {
			AbortProtocolError("malformed PREMATURE packet");
			return false;
		}

		try {
			input.Premature(LoadUnaligned<uint64_t>(payload.data()));
		} catch (...) {
			AbortError(std::current_exception());
			return false;
		}

		if (stopping)
			stopping = true;
		else
			response_handler->OnWasError(std::make_exception_ptr(std::runtime_error{"Premature end of response body"}));
		return true;

	case WAS_COMMAND_METRIC:
		break;
	}

	return true;
}

bool
SimpleClient::OnWasControlDrained() noexcept
{
	if (state == State::BODY) {
		response.body = input.CheckComplete();
		if (response.body) {
			state = State::IDLE;
			response_handler->OnWasResponse(std::move(response));
		}
	}

	return true;
}

void
SimpleClient::OnWasControlDone() noexcept
{
	assert(control.IsDefined());
}

void
SimpleClient::OnWasControlHangup() noexcept
{
	assert(!control.IsDefined());

	AbortError(std::make_exception_ptr(SocketClosedPrematurelyError{}));
}

void
SimpleClient::OnWasControlError(std::exception_ptr error) noexcept
{
	assert(control.IsDefined());

	AbortError(error);
}

void
SimpleClient::OnWasInput(DisposableBuffer body) noexcept
{
	assert(state == State::BODY);

	response.body = std::move(body);
	state = State::IDLE;
	response_handler->OnWasResponse(std::move(response));
}

void
SimpleClient::OnWasInputHangup() noexcept
{
	AbortError(std::make_exception_ptr(SocketClosedPrematurelyError{}));
}

void
SimpleClient::OnWasInputError(std::exception_ptr error) noexcept
{
	AbortError(error);
}

void
SimpleClient::OnWasOutputError(std::exception_ptr error) noexcept
{
	AbortError(error);
}

void
SimpleClient::Cancel() noexcept
{
	assert(state != State::IDLE);

	if (output.IsActive()) {
		if (!control.SendUint64(WAS_COMMAND_PREMATURE,
					output.Stop()))
			return;
	}

	// TODO cancel input?

	if (!control.Send(WAS_COMMAND_STOP))
		return;

	stopping = true;
	state = State::IDLE;
}

} // namespace Was
