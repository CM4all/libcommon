// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "QmqpClient.hxx"
#include "util/SpanCast.hxx"

#include <string> // TODO: migrate to std::string_view

using std::string_view_literals::operator""sv;

void
QmqpClient::AppendNetstring(std::string_view value) noexcept
{
	netstring_headers.emplace_front();
	auto &g = netstring_headers.front();
	request.emplace_back(AsBytes(g(value.size())));
	request.emplace_back(AsBytes(value));
	request.emplace_back(AsBytes(","sv));
}

void
QmqpClient::Commit(FileDescriptor out_fd, FileDescriptor in_fd) noexcept
{
	assert(!netstring_headers.empty());
	assert(!request.empty());

	client.Request(out_fd, in_fd, std::move(request));
}

void
QmqpClient::OnNetstringResponse(AllocatedArray<std::byte> &&_payload) noexcept
try {
	std::string_view payload = ToStringView(_payload);

	if (!payload.empty()) {
		switch (payload[0]) {
		case 'K':
			// success
			handler.OnQmqpClientSuccess(payload.substr(1));
			return;

		case 'Z':
			// temporary failure
			throw QmqpClientTemporaryFailure(std::string{payload.substr(1)});

		case 'D':
			// permanent failure
			throw QmqpClientPermanentFailure(std::string{payload.substr(1)});
		}
	}

	throw QmqpClientError("Malformed QMQP response");
} catch (...) {
	handler.OnQmqpClientError(std::current_exception());
}
