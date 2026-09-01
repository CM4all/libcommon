// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "SimpleServer.hxx"
#include "SimpleResponse.hxx"
#include "Socket.hxx"
#include "net/SocketProtocolError.hxx"
#include "util/SpanCast.hxx"
#include "util/StringSplit.hxx"
#include "util/Unaligned.hxx"

#include <array>

namespace Was {

SimpleServer::SimpleServer(EventLoop &event_loop, WasSocket &&socket,
			   SimpleServerHandler &_handler,
			   SimpleRequestHandler &_request_handler) noexcept
	:control(event_loop, std::move(socket.control), *this),
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

SimpleServer::~SimpleServer() noexcept
{
	CancelRequest();
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
	Close();
	handler.OnWasClosed(*this);
}

void
SimpleServer::AbortError(std::exception_ptr error) noexcept
{
	CancelRequest();
	Close();
	handler.OnWasError(*this, error);
}

void
SimpleServer::AbortProtocolError(const char *msg) noexcept
{
	AbortError(std::make_exception_ptr(SocketProtocolError{msg}));
}

template<typename E, std::integral I>
requires std::is_enum_v<E>
[[nodiscard]]
static bool
CastToEnumWithRangeCheck(E &result, I src) noexcept
{
	bool success = std::in_range<std::underlying_type_t<E>>(src);
	if (success)
		result = static_cast<E>(src);
	return success;
}

template<std::unsigned_integral I, typename E>
requires std::is_enum_v<E>
[[nodiscard]]
static bool
LoadCheckUnalignedEnum(E &result, const void *src) noexcept
{
	I i;
	LoadUnaligned(i, src);
	return CastToEnumWithRangeCheck(result, i);
}

inline bool
SimpleServer::OnWasControlMethod(std::span<const std::byte> payload) noexcept
{
	if (request.state != Request::State::HEADERS) {
		AbortProtocolError("misplaced METHOD packet");
		return false;
	}

	if (request.have_method) {
		AbortProtocolError("duplicate METHOD packet");
		return false;
	}

	HttpMethod method;
	bool load_success;

	if (payload.size() == sizeof(uint16_t)) {
		/* documented payload size is 16 bit */
		load_success = LoadCheckUnalignedEnum<uint16_t>(method, payload.data());
	} else if (payload.size() == sizeof(uint32_t)) {
		/* accept 32 bit as well (for buggy clients which send the
		   enum) */
		load_success = LoadCheckUnalignedEnum<uint32_t>(method, payload.data());
	} else {
		AbortProtocolError("malformed METHOD packet");
		return false;
	}

	if (!load_success || !http_method_is_valid(method)) {
		AbortProtocolError("invalid METHOD packet");
		return false;
	}

	request.method = request.request->method = method;
	request.have_method = true;
	return true;
}

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
		request.method = HttpMethod::GET;
		request.have_method = false;
		request.state = Request::State::HEADERS;
		output.ResetPosition();
		break;

	case WAS_COMMAND_METHOD:
		return OnWasControlMethod(payload);

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
		if (request.state != Request::State::HEADERS) {
			AbortProtocolError("misplaced SCRIPT_NAME packet");
			return false;
		}

		request.request->script_name.assign((const char *)payload.data(),
						    payload.size());
		break;

	case WAS_COMMAND_PATH_INFO:
		if (request.state != Request::State::HEADERS) {
			AbortProtocolError("misplaced PATH_INFO packet");
			return false;
		}

		request.request->path_info.assign((const char *)payload.data(),
						    payload.size());
		break;

	case WAS_COMMAND_QUERY_STRING:
		if (request.state != Request::State::HEADERS) {
			AbortProtocolError("misplaced QUERY_STRING packet");
			return false;
		}

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
		if (input.IsActive()) {
			/* the request body is still being received;
			   discard the rest of it, or else its leftovers
			   in the pipe would be read as the body of the
			   next request */
			try {
				if (!input.Discard()) {
					/* without LENGTH, the peer has to
					   send PREMATURE before STOP to
					   allow resynchronizing the pipe */
					AbortProtocolError("STOP without LENGTH or PREMATURE");
					return false;
				}
			} catch (...) {
				AbortError(std::current_exception());
				return false;
			}
		}

		if (CancelRequest())
			/* the handler was canceled before it could
			   produce a response */
			return control.SendUint64(WAS_COMMAND_PREMATURE, 0);

		return control.SendUint64(WAS_COMMAND_PREMATURE,
					  output.Stop());

	case WAS_COMMAND_PREMATURE:
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

		if (request.state == Request::State::BODY)
			CancelRequest();

		return true;

	case WAS_COMMAND_REMOTE_HOST:
		if (request.state != Request::State::HEADERS) {
			AbortProtocolError("misplaced REMOTE_HOST packet");
			return false;
		}

		request.request->remote_host.assign((const char *)payload.data(),
						    payload.size());
		break;

	case WAS_COMMAND_DOCUMENT_ROOT:
		if (request.state != Request::State::HEADERS) {
			AbortProtocolError("misplaced DOCUMENT_ROOT packet");
			return false;
		}

		request.request->document_root.assign((const char *)payload.data(),
						      payload.size());
		break;

	case WAS_COMMAND_TLS:
		if (request.state != Request::State::HEADERS) {
			AbortProtocolError("misplaced TLS packet");
			return false;
		}

		request.request->tls = true;
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
	assert(control.IsDefined());
}

void
SimpleServer::OnWasControlHangup() noexcept
{
	assert(!control.IsDefined());

	Closed();
}

void
SimpleServer::OnWasControlError(std::exception_ptr error) noexcept
{
	assert(control.IsDefined());

	AbortError(error);
}

bool
SimpleServer::OnWasOutputLength(uint_least64_t length) noexcept
{
	return control.SendUint64(WAS_COMMAND_LENGTH, length);
}

void
SimpleServer::OnWasOutputEnd() noexcept
{
	output.Deactivate();
}

void
SimpleServer::OnWasOutputError(std::exception_ptr &&error) noexcept
{
	AbortError(std::move(error));
}

void
SimpleServer::OnWasInput(DisposableBuffer body) noexcept
{
	assert(request.state == Request::State::BODY);

	request.request->body = std::move(body);
	SubmitRequest();
}

void
SimpleServer::OnWasInputHangup() noexcept
{
	Closed();
}

void
SimpleServer::OnWasInputError(std::exception_ptr error) noexcept
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
		/* TODO
		if (request.method == HttpMethod::HEAD)
			response.headers.emplace("content-length",
						 fmt::format_int{response.body.size()}.c_str());
		*/

		response.body = {};
	}

	for (const auto &i : response.headers)
		if (!control.SendPair(WAS_COMMAND_HEADER, i.first, i.second))
			return false;

	if (response.body) {
		if (!control.Send(WAS_COMMAND_DATA))
			return false;

		if (!output.Activate(std::move(response.body)))
			return false;
	} else {
		if (!control.Send(WAS_COMMAND_NO_DATA))
			return false;
	}

	return true;
}

} // namespace Was
