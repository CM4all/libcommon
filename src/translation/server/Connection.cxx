// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Connection.hxx"
#include "Handler.hxx"
#include "Response.hxx"
#include "translation/Protocol.hxx"
#include "io/Logger.hxx"

#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

namespace Translation::Server {

Connection::Connection(EventLoop &event_loop,
		       Handler &_handler,
		       UniqueSocketDescriptor &&_fd) noexcept
	:handler(_handler),
	 event(event_loop, BIND_THIS_METHOD(OnSocketReady), _fd.Release()),
	 state(State::INIT),
	 input(8192)
{
	event.ScheduleRead();
}

Connection::~Connection() noexcept
{
	if (state == State::RESPONSE)
		delete[] response;

	if (cancel_ptr)
		cancel_ptr.Cancel();

	event.Close();
}

inline bool
Connection::TryRead() noexcept
{
	assert(state == State::INIT || state == State::REQUEST);

	auto r = input.Write();
	assert(!r.empty());

	ssize_t nbytes = recv(event.GetSocket().Get(), r.data(), r.size(),
			      MSG_DONTWAIT);
	if (nbytes > 0) [[likely]] {
		input.Append(nbytes);
		return OnReceived();
	}

	if (nbytes < 0) {
		if (errno == EAGAIN)
			return true;

		LogConcat(2, "ts", "Failed to read from client: ", strerror(errno));
	}

	Destroy();
	return false;
}

inline bool
Connection::OnReceived() noexcept
{
	assert(state != State::PROCESSING);

	while (true) {
		auto r = input.Read();
		const void *p = r.data();
		const auto *header = (const TranslationHeader *)p;
		if (r.size() < sizeof(*header))
			break;

		const size_t payload_length = header->length;
		const size_t total_size = sizeof(*header) + payload_length;
		if (r.size() < total_size)
			break;

		const auto payload = r.subspan(sizeof(*header),
					       payload_length);
		if (!OnPacket(header->command, payload))
			return false;

		input.Consume(total_size);
	}

	return true;
}

inline bool
Connection::OnPacket(TranslationCommand cmd,
		     std::span<const std::byte> payload) noexcept
{
	assert(state != State::PROCESSING);

	if (cancel_ptr) {
		LogConcat(1, "ts",
			  "Received more request packets while another request is still pending");
		Destroy();
		return false;
	}

	if (cmd == TranslationCommand::BEGIN) {
		if (state != State::INIT) {
			LogConcat(1, "ts", "Misplaced INIT");
			Destroy();
			return false;
		}

		state = State::REQUEST;
	}

	if (state != State::REQUEST) {
		LogConcat(1, "ts", "INIT expected");
		Destroy();
		return false;
	}

	if (cmd == TranslationCommand::END) [[unlikely]] {
		state = State::PROCESSING;
		return handler.OnTranslationRequest(*this, request, cancel_ptr);
	}

	try {
		request.Parse(cmd, payload);
	} catch (...) {
		LogConcat(1, "ts", std::current_exception());
		Destroy();
		return false;
	}

	return true;
}

bool
Connection::TryWrite() noexcept
{
	assert(state == State::RESPONSE);

	ssize_t nbytes = event.GetSocket().Send(output, MSG_DONTWAIT);
	if (nbytes < 0) {
		if (errno == EAGAIN) [[likely]] {
			event.ScheduleWrite();
			return true;
		}

		LogConcat(2, "ts", "Failed to write to client: ", strerror(errno));
		Destroy();
		return false;
	}

	output = output.subspan(nbytes);

	if (output.empty()) {
		delete[] response;
		state = State::INIT;
		event.CancelWrite();
	}

	return true;
}

bool
Connection::SendResponse(Response &&_response) noexcept
{
	assert(state == State::PROCESSING);

	state = State::RESPONSE;
	output = _response.Finish();
	response = output.data();
	cancel_ptr = nullptr;

	return TryWrite();
}

void
Connection::OnSocketReady(unsigned events) noexcept
{
	if (events & SocketEvent::HANGUP) {
		Destroy();
		return;
	}

	if (events & SocketEvent::READ) {
		if (!TryRead())
			return;
	}

	if (events & SocketEvent::WRITE)
		TryWrite();
}

} // namespace Translation::Server
