/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef PG_REFLECTION_HXX
#define PG_REFLECTION_HXX

namespace Pg {

class Connection;

/**
 * Does a column with the specified name exist in the table?
 *
 * @param schema the schema name (must not be nullptr or empty, as
 * there is no fallback to "public")
 */
bool
ColumnExists(Connection &c, const char *schema,
	     const char *table_name, const char *column_name);

/**
 * Does an index with the specified name exist in the table?
 *
 * @param schema the schema name (must not be nullptr or empty, as
 * there is no fallback to "public")
 */
bool
IndexExists(Connection &c, const char *schema,
	    const char *table_name, const char *index_name);

}

#endif
