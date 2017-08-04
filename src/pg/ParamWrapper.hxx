/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef PG_PARAM_WRAPPER_HXX
#define PG_PARAM_WRAPPER_HXX

#include "Serial.hxx"
#include "BinaryValue.hxx"
#include "Array.hxx"

#include <iterator>
#include <cinttypes>
#include <cstdio>
#include <cstddef>

namespace Pg {

template<typename T, typename Enable=void>
struct ParamWrapper {
	ParamWrapper(const T &t);
	const char *GetValue() const;

	/**
	 * Is the buffer returned by GetValue() binary?  If so, the method
	 * GetSize() must return the size of the value.
	 */
	bool IsBinary() const;

	/**
	 * Returns the size of the value in bytes.  Only applicable if
	 * IsBinary() returns true and the value is non-nullptr.
	 */
	size_t GetSize() const;
};

template<>
struct ParamWrapper<Serial> {
	char buffer[16];

	ParamWrapper(Serial s) {
		sprintf(buffer, "%" PRIpgserial, s.get());
	}

	const char *GetValue() const {
		return buffer;
	}

	static constexpr bool IsBinary() {
		return false;
	}

	size_t GetSize() const {
		/* ignored for text columns */
		return 0;
	}
};

template<>
struct ParamWrapper<BinaryValue> {
	BinaryValue value;

	constexpr ParamWrapper(BinaryValue _value)
	:value(_value) {}

	constexpr const char *GetValue() const {
		return (const char *)value.data;
	}

	static constexpr bool IsBinary() {
		return true;
	}

	constexpr size_t GetSize() const {
		return value.size;
	}
};

template<>
struct ParamWrapper<const char *> {
	const char *value;

	constexpr ParamWrapper(const char *_value):value(_value) {}

	constexpr const char *GetValue() const {
		return value;
	}

	static constexpr bool IsBinary() {
		return false;
	}

	size_t GetSize() const {
		/* ignored for text columns */
		return 0;
	}
};

template<>
struct ParamWrapper<int> {
	char buffer[16];

	ParamWrapper(int i) {
		sprintf(buffer, "%i", i);
	}

	const char *GetValue() const {
		return buffer;
	}

	static constexpr bool IsBinary() {
		return false;
	}

	size_t GetSize() const {
		/* ignored for text columns */
		return 0;
	}
};

template<>
struct ParamWrapper<int64_t> {
	char buffer[32];

	ParamWrapper(int64_t i) {
		sprintf(buffer, "%" PRId64, i);
	}

	const char *GetValue() const {
		return buffer;
	}

	static constexpr bool IsBinary() {
		return false;
	}

	size_t GetSize() const {
		/* ignored for text columns */
		return 0;
	}
};

template<>
struct ParamWrapper<unsigned> {
	char buffer[16];

	ParamWrapper(unsigned i) {
		sprintf(buffer, "%u", i);
	}

	const char *GetValue() const {
		return buffer;
	}

	static constexpr bool IsBinary() {
		return false;
	}

	size_t GetSize() const {
		/* ignored for text columns */
		return 0;
	}
};

template<>
struct ParamWrapper<bool> {
	const char *value;

	constexpr ParamWrapper(bool _value):value(_value ? "t" : "f") {}

	static constexpr bool IsBinary() {
		return false;
	}

	constexpr const char *GetValue() const {
		return value;
	}

	size_t GetSize() const {
		/* ignored for text columns */
		return 0;
	}
};

/**
 * Specialization for STL container types of std::string instances.
 */
template<typename T>
struct ParamWrapper<T,
		    std::enable_if_t<std::is_same<typename T::value_type,
						  std::string>::value>> {
	std::string value;

	ParamWrapper(const T &list)
		:value(EncodeArray(list)) {}

	static constexpr bool IsBinary() {
		return false;
	}

	const char *GetValue() const {
		return value.c_str();
	}

	size_t GetSize() const {
		/* ignored for text columns */
		return 0;
	}
};

template<>
struct ParamWrapper<const std::list<std::string> *> {
	std::string value;

	ParamWrapper(const std::list<std::string> *list)
		:value(list != nullptr
		       ? EncodeArray(*list)
		       : std::string()) {}

	static constexpr bool IsBinary() {
		return false;
	}

	const char *GetValue() const {
		return value.empty() ? nullptr : value.c_str();
	}

	size_t GetSize() const {
		/* ignored for text columns */
		return 0;
	}
};

template<typename... Params>
class ParamCollector;

template<>
class ParamCollector<> {
public:
	static constexpr size_t Count() {
		return 0;
	}

	template<typename O>
	size_t Fill(O) const {
		return 0;
	}
};

template<typename T>
class ParamCollector<T> {
	ParamWrapper<T> wrapper;

public:
	explicit ParamCollector(const T &t):wrapper(t) {}

	static constexpr size_t Count() {
		return 1;
	}

	template<typename O, typename S, typename F>
	size_t Fill(O output, S size, F format) const {
		*output = wrapper.GetValue();
		*size = wrapper.GetSize();
		*format = wrapper.IsBinary();
		return 1;
	}

	template<typename O>
	size_t Fill(O output) const {
		static_assert(!decltype(wrapper)::IsBinary(),
			      "Binary values not allowed in this overload");

		*output = wrapper.GetValue();
		return 1;
	}
};

template<typename T, typename... Rest>
class ParamCollector<T, Rest...> {
	ParamCollector<T> first;
	ParamCollector<Rest...> rest;

public:
	explicit ParamCollector(const T &t, Rest... _rest)
		:first(t), rest(_rest...) {}

	static constexpr size_t Count() {
		return decltype(first)::Count() + decltype(rest)::Count();
	}

	template<typename O, typename S, typename F>
	size_t Fill(O output, S size, F format) const {
		const size_t nf = first.Fill(output, size, format);
		std::advance(output, nf);
		std::advance(size, nf);
		std::advance(format, nf);

		const size_t nr = rest.Fill(output, size, format);
		return nf + nr;
	}

	template<typename O>
	size_t Fill(O output) const {
		const size_t nf = first.Fill(output);
		std::advance(output, nf);

		const size_t nr = rest.Fill(output);
		return nf + nr;
	}
};

template<typename... Params>
class TextParamArray {
	ParamCollector<Params...> collector;

public:
	static constexpr size_t count = decltype(collector)::Count();
	const char *values[count];

	explicit TextParamArray(Params... params):collector(params...) {
		collector.Fill(values);
	}
};

template<typename... Params>
class BinaryParamArray {
	ParamCollector<Params...> collector;

public:
	static constexpr size_t count = decltype(collector)::Count();
	const char *values[count];
	int lengths[count], formats[count];

	explicit BinaryParamArray(Params... params):collector(params...) {
		collector.Fill(values, lengths, formats);
	}
};

} /* namespace Pg */

#endif
