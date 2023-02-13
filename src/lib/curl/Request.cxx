// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Request.hxx"
#include "Setup.hxx"
#include "Global.hxx"

#include <curl/curl.h>

#include <cassert>

CurlRequest::CurlRequest(CurlGlobal &_global, CurlEasy _easy,
			 CurlResponseHandler &_handler)
	:global(_global), handler(_handler), easy(std::move(_easy))
{
	SetupEasy();
}

CurlRequest::CurlRequest(CurlGlobal &_global,
			 CurlResponseHandler &_handler)
	:global(_global), handler(_handler)
{
	SetupEasy();
}

CurlRequest::~CurlRequest() noexcept
{
	FreeEasy();
}

void
CurlRequest::SetupEasy()
{
	easy.SetPrivate((void *)this);

	handler.Install(easy);

	Curl::Setup(easy);
}

void
CurlRequest::Start()
{
	assert(!registered);

	global.Add(*this);
	registered = true;
}

void
CurlRequest::Stop() noexcept
{
	if (!registered)
		return;

	global.Remove(*this);
	registered = false;
}

void
CurlRequest::FreeEasy() noexcept
{
	if (!easy)
		return;

	Stop();
	easy = nullptr;
}

void
CurlRequest::Resume() noexcept
{
	assert(registered);

	easy.Unpause();

	global.InvalidateSockets();
}

void
CurlRequest::Done(CURLcode result) noexcept
{
	Stop();

	handler.Done(result);
}
