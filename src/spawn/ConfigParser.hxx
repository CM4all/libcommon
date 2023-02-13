// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "io/config/ConfigParser.hxx"

struct SpawnConfig;

class SpawnConfigParser final : public ConfigParser {
	SpawnConfig &config;

public:
	explicit SpawnConfigParser(SpawnConfig &_config):config(_config) {}

protected:
	/* virtual methods from class ConfigParser */
	void ParseLine(FileLineParser &line) override;
};
