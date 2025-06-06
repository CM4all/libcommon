// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Reflection.hxx"
#include "Connection.hxx"
#include "lib/fmt/RuntimeError.hxx"

namespace Pg {

bool
TableExists(Pg::Connection &c, const char *schema, const char *table_name)
{
	assert(schema != nullptr);
	assert(*schema != 0);
	assert(table_name != nullptr);

	return c.ExecuteParams("SELECT 1 FROM INFORMATION_SCHEMA.TABLES "
			       "WHERE table_schema=$1 AND table_name=$2 AND table_type='BASE TABLE'",
			       schema, table_name).GetRowCount() > 0;

}

bool
ColumnExists(Pg::Connection &c, const char *schema,
	     const char *table_name, const char *column_name)
{
	assert(schema != nullptr);
	assert(*schema != 0);

	return c.ExecuteParams("SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS "
			       "WHERE table_schema=$1 AND table_name=$2 AND column_name=$3",
			       schema, table_name, column_name).GetRowCount() > 0;

}

std::string
GetColumnType(Pg::Connection &c, const char *schema,
	      const char *table_name, const char *column_name)
{
	assert(schema != nullptr);
	assert(*schema != 0);

	const auto result =
		c.ExecuteParams("SELECT data_type FROM INFORMATION_SCHEMA.COLUMNS "
				"WHERE table_schema=$1 AND table_name=$2 AND column_name=$3",
				schema, table_name, column_name);
	if (result.GetRowCount() == 0)
		throw FmtRuntimeError("No such column: {}", column_name);

	return result.GetValue(0, 0);

}

bool
IndexExists(Pg::Connection &c, const char *schema,
	    const char *table_name, const char *index_name)
{
	assert(schema != nullptr);
	assert(*schema != 0);

	return c.ExecuteParams("SELECT 1 FROM pg_indexes "
			       "WHERE schemaname=$1 AND tablename=$2 AND indexname=$3",
			       schema, table_name, index_name).GetRowCount() > 0;
}

bool
RuleExists(Pg::Connection &c, const char *schema,
	   const char *table_name, const char *rule_name)
{
	assert(schema != nullptr);
	assert(*schema != 0);
	assert(table_name != nullptr);
	assert(rule_name != nullptr);

	return c.ExecuteParams("SELECT 1 FROM pg_rules "
			       "WHERE schemaname=$1 AND tablename=$2 AND rulename=$3",
			       schema, table_name, rule_name).GetRowCount() > 0;

}

}
