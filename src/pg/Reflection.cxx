/*
 * Copyright 2007-2017 Content Management AG
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "Reflection.hxx"
#include "Connection.hxx"
#include "CheckError.hxx"
#include "util/RuntimeError.hxx"

namespace Pg {

bool
TableExists(Pg::Connection &c, const char *schema, const char *table_name)
{
	assert(schema != nullptr);
	assert(*schema != 0);
	assert(table_name != nullptr);

	return CheckError(c.ExecuteParams("SELECT 1 FROM INFORMATION_SCHEMA.TABLES "
					  "WHERE table_schema=$1 AND table_name=$2 AND table_type='BASE TABLE'",
					  schema, table_name)).GetRowCount() > 0;

}

bool
ColumnExists(Pg::Connection &c, const char *schema,
	     const char *table_name, const char *column_name)
{
	assert(schema != nullptr);
	assert(*schema != 0);

	return CheckError(c.ExecuteParams("SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS "
					  "WHERE table_schema=$1 AND table_name=$2 AND column_name=$3",
					  schema, table_name, column_name)).GetRowCount() > 0;

}

std::string
GetColumnType(Pg::Connection &c, const char *schema,
	      const char *table_name, const char *column_name)
{
	assert(schema != nullptr);
	assert(*schema != 0);

	const auto result =
		CheckError(c.ExecuteParams("SELECT data_type FROM INFORMATION_SCHEMA.COLUMNS "
					   "WHERE table_schema=$1 AND table_name=$2 AND column_name=$3",
					   schema, table_name, column_name));
	if (result.GetRowCount() == 0)
		throw FormatRuntimeError("No such column: %s", column_name);

	return result.GetValue(0, 0);

}

bool
IndexExists(Pg::Connection &c, const char *schema,
	    const char *table_name, const char *index_name)
{
	assert(schema != nullptr);
	assert(*schema != 0);

	return CheckError(c.ExecuteParams("SELECT 1 FROM pg_indexes "
					  "WHERE schemaname=$1 AND tablename=$2 AND indexname=$3",
					  schema, table_name, index_name)).GetRowCount() > 0;
}

bool
RuleExists(Pg::Connection &c, const char *schema,
	   const char *table_name, const char *rule_name)
{
	assert(schema != nullptr);
	assert(*schema != 0);
	assert(table_name != nullptr);
	assert(rule_name != nullptr);

	return CheckError(c.ExecuteParams("SELECT 1 FROM pg_rules "
					  "WHERE schemaname=$1 AND tablename=$2 AND rulename=$3",
					  schema, table_name, rule_name)).GetRowCount() > 0;

}

}
