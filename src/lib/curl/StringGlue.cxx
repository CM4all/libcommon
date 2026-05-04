// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "StringGlue.hxx"
#include "StringHandler.hxx"
#include "Adapter.hxx"
#include "Easy.hxx"

namespace Curl {

StringResponse
StringRequest(CurlEasy easy)
{
	StringResponseHandler handler;
	CurlResponseHandlerAdapter adapter{handler};
	adapter.Install(easy);

	CURLcode code = curl_easy_perform(easy.Get());
	adapter.Done(code);

	handler.CheckRethrowError();

	if (code != CURLE_OK)
		throw Curl::MakeError(code, "CURL error");

	return std::move(handler).TakeResponse();
}

} // namespace Curl
