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

	bool IsDefaultSchema() const noexcept {
		using std::string_view_literals::operator""sv;
		return schema.empty() || schema == "public"sv;
	}

	/**
	 * Returns the name of the PostgreSQL schema that is
	 * effectively being used.  If no schema was passed to the
	 * constructor, this method returns the PostgreSQL default
	 * schema name "public".
	 */
	const char *GetEffectiveSchemaName() const noexcept {
		return schema.empty() ? "public" : schema.c_str();
	}
};

} /* namespace Pg */
