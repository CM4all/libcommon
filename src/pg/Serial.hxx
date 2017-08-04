/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef PG_SERIAL_HXX
#define PG_SERIAL_HXX

#include <stdint.h>

#define PRIpgserial PRId32

namespace Pg {

/**
 * C++ representation of a PostgreSQL "serial" value.
 */
class Serial {
	typedef int32_t value_type;
	value_type value;

public:
	Serial() = default;
	explicit constexpr Serial(value_type _value):value(_value) {}

	constexpr value_type get() const {
		return value;
	}

	constexpr operator bool() const {
		return value != 0;
	}

	/**
	 * Convert a string to a #Serial instance.  Throws
	 * std::invalid_argument on error.
	 */
	static Serial Parse(const char *s);
};

}

#endif
