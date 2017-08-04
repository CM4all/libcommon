/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef PG_CHECK_ERROR_HXX
#define PG_CHECK_ERROR_HXX

#include "Result.hxx"
#include "Error.hxx"

namespace Pg {

/**
 * Check if the #Result contains an error state, and throw a
 * #Error based on this condition.  If the #Result did not contain
 * an error, it is returned as-is.
 */
static inline Result
CheckError(Result &&result)
{
	if (result.IsError())
		throw Error(std::move(result));

	return std::move(result);
}

} /* namespace Pg */

#endif
