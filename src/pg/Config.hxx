// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <string>

namespace Pg {

/**
 * Instructions for setting up a PostgreSQL database connection.
 * These values are usually read from a configuration file.
 */
struct Config {
	/**
	 * The connect string ("conninfo").
	 */
	std::string connect;

	/**
	 * The schema name.  If empty, the default schema "public" is
	 * used.
	 */
	std::string schema;
};

} /* namespace Pg */
