// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <string>

namespace Pg {

class Connection;

/**
 * Does the specified table exist?
 *
 * Throws on error.
 *
 * @param schema the schema name (must not be nullptr or empty, as
 * there is no fallback to "public")
 */
bool
TableExists(Connection &c, const char *schema,
	    const char *table_name);

/**
 * Does a column with the specified name exist in the table?
 *
 * Throws on error.
 *
 * @param schema the schema name (must not be nullptr or empty, as
 * there is no fallback to "public")
 */
bool
ColumnExists(Connection &c, const char *schema,
	     const char *table_name, const char *column_name);

/**
 * Throws on error.
 */
std::string
GetColumnType(Connection &c, const char *schema,
	      const char *table_name, const char *column_name);

/**
 * Does an index with the specified name exist in the table?
 *
 * Throws on error.
 *
 * @param schema the schema name (must not be nullptr or empty, as
 * there is no fallback to "public")
 */
bool
IndexExists(Connection &c, const char *schema,
	    const char *table_name, const char *index_name);

/**
 * Does a rule with the specified name exist in the table?
 *
 * Throws on error.
 *
 * @param schema the schema name (must not be nullptr or empty, as
 * there is no fallback to "public")
 */
bool
RuleExists(Pg::Connection &c, const char *schema,
	   const char *table_name, const char *rule_name);

}
