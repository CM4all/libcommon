// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "StringGlue.hxx"
#include "StringHandler.hxx"
#include "Adapter.hxx"
#include "Easy.hxx"

StringCurlResponse
StringCurlRequest(CurlEasy easy)
{
	StringCurlResponseHandler handler;
	CurlResponseHandlerAdapter adapter{handler};
	adapter.Install(easy);

	CURLcode code = curl_easy_perform(easy.Get());
	adapter.Done(code);

	handler.CheckRethrowError();

	if (code != CURLE_OK)
		throw Curl::MakeError(code, "CURL error");

	return std::move(handler).GetResponse();
}
