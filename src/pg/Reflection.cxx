/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Reflection.hxx"
#include "Connection.hxx"
#include "CheckError.hxx"

namespace Pg {

bool
ColumnExists(Pg::Connection &c, const char *schema,
	     const char *table_name, const char *column_name)
{
	assert(schema != nullptr);
	assert(*schema != 0);

	return CheckError(c.ExecuteParams("SELECT data_type FROM INFORMATION_SCHEMA.COLUMNS "
					  "WHERE table_schema=$1 AND table_name=$2 AND column_name=$3",
					  schema, table_name, column_name)).GetRowCount() > 0;

}

bool
IndexExists(Pg::Connection &c, const char *schema,
	    const char *table_name, const char *index_name)
{
	assert(schema != nullptr);
	assert(*schema != 0);

	return CheckError(c.ExecuteParams("SELECT indexdef FROM pg_indexes "
					  "WHERE schemaname=$1 AND tablename=$2 AND indexname=$3",
					  schema, table_name, index_name)).GetRowCount() > 0;
}

}
