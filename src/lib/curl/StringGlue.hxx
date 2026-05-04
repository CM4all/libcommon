// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "StringResponse.hxx"

class CurlEasy;

namespace Curl {

StringResponse
StringRequest(CurlEasy easy);

} // namespace Curl
