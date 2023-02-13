// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <boost/json/fwd.hpp>

struct was_simple;

boost::json::value
ReadJsonRequestBody(struct was_simple *w);
