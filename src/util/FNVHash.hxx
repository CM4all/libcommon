/*
 * Implementation of the Fowler-Noll-Vo hash function.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef FNV_HASH_HXX
#define FNV_HASH_HXX

#include "Compiler.h"

#include <stddef.h>
#include <stdint.h>

template<typename T>
struct FNVTraits {};

template<>
struct FNVTraits<uint32_t> {
	typedef uint32_t value_type;
	typedef uint_fast32_t fast_type;

	static constexpr value_type OFFSET_BASIS = 2166136261u;
	static constexpr value_type PRIME = 16777619u;
};

template<>
struct FNVTraits<uint64_t> {
	typedef uint64_t value_type;
	typedef uint_fast64_t fast_type;

	static constexpr value_type OFFSET_BASIS = 14695981039346656037u;
	static constexpr value_type PRIME = 1099511628211u;
};

template<typename Traits>
struct FNV1aAlgorithm {
	typedef typename Traits::value_type value_type;
	typedef typename Traits::fast_type fast_type;

	gcc_hot
	static constexpr fast_type Update(fast_type hash, uint8_t b) {
		return (hash ^ b) * Traits::PRIME;
	}

	gcc_pure gcc_hot
	static value_type StringHash(const char *s) {
		using Algorithm = FNV1aAlgorithm<Traits>;

		fast_type hash = Traits::OFFSET_BASIS;
		while (*s)
			hash = Algorithm::Update(hash, *s++);

		return hash;
	}
};

gcc_pure gcc_hot
inline uint32_t
FNV1aHash32(const char *s)
{
	using Traits = FNVTraits<uint32_t>;
	using Algorithm = FNV1aAlgorithm<Traits>;
	return Algorithm::StringHash(s);
}

gcc_pure gcc_hot
inline uint64_t
FNV1aHash64(const char *s)
{
	using Traits = FNVTraits<uint64_t>;
	using Algorithm = FNV1aAlgorithm<Traits>;
	return Algorithm::StringHash(s);
}

gcc_pure gcc_hot
inline uint32_t
FNV1aHashFold32(const char *s)
{
	const uint64_t h64 = FNV1aHash64(s);

	/* XOR folding */
	const uint_fast32_t lo(h64);
	const uint_fast32_t hi(h64 >> 32);
	return lo ^ hi;
}

#endif
