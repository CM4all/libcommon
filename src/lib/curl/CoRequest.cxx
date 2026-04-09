// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "CoRequest.hxx"
#include "net/SocketProtocolError.hxx"
#include "util/SpanCast.hxx"

namespace Curl {

CoRequest::CoRequest(CurlGlobal &global, CurlEasy easy,
		     CoOptions _options)
	:request(global, std::move(easy), *this),
	 options(_options)
{
	request.Start();
}

void
CoRequest::OnHeaders(HttpStatus status, Headers &&headers)
{
	response.status = status;
	response.headers = std::move(headers);
}

void
CoRequest::OnData(std::span<const std::byte> data)
{
	if (response.body.size() + data.size() > options.max_size)
		throw SocketMessageTooLargeError{"Response body is too large"};

	response.body.append(ToStringView(data));
}

void
CoRequest::OnEnd()
{
	ready = true;

	if (continuation)
		continuation.resume();
}

void
CoRequest::OnError(std::exception_ptr e) noexcept
{
	error = std::move(e);
	ready = true;

	if (continuation)
		continuation.resume();
}

} // namespace Curl
