// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Setup.hxx"
#include "Easy.hxx"
#include "version.h"

namespace Curl {

void
Setup(CurlEasy &easy)
{
	easy.SetUserAgent(PACKAGE " " VERSION);
	easy.SetNoProgress();
	easy.SetNoSignal();
	easy.SetConnectTimeout(10);
}

} // namespace Curl
