// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "lib/fmt/SystemError.hxx"

#include "seccomp.h"

#include <utility>

namespace Seccomp {

class Filter {
	const scmp_filter_ctx ctx;

public:
	/**
	 * Throws std::runtime_error on error.
	 */
	explicit Filter(uint32_t def_action);

	~Filter() {
		seccomp_release(ctx);
	}

	Filter(const Filter &) = delete;
	Filter &operator=(const Filter &) = delete;

	/**
	 * Throws std::runtime_error on error.
	 */
	void Reset(uint32_t def_action);

	void Load() const;

	void SetAttributeNoThrow(enum scmp_filter_attr attr, uint32_t value) noexcept {
		seccomp_attr_set(ctx, attr, value);
	}

	void AddArch(uint32_t arch_token);
	void AddSecondaryArchs() noexcept;

	template<typename... Args>
	void AddRule(uint32_t action, int syscall, Args... args) {
		int error = seccomp_rule_add(ctx, action, syscall, sizeof...(args),
					     std::forward<Args>(args)...);
		if (error < 0)
			throw FmtErrno(-error, "seccomp_rule_add({}) failed", syscall);
	}
};

#ifdef __clang__
/* work around "error: missing field 'datum_b' initializer" in
   SCMP_CMP() */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
/* work around "compound literals are a C99-specific feature" */
#pragma GCC diagnostic ignored "-Wc99-extensions"
#endif

/**
 * Reference to a system call argument.
 */
class Arg {
	unsigned arg;

public:
	explicit constexpr Arg(unsigned _arg):arg(_arg) {}

	constexpr auto Cmp(enum scmp_compare op, scmp_datum_t datum) const {
		return SCMP_CMP(arg, op, datum);
	}

	constexpr auto operator==(scmp_datum_t datum) const {
		return Cmp(SCMP_CMP_EQ, datum);
	}

	constexpr auto operator!=(scmp_datum_t datum) const {
		return Cmp(SCMP_CMP_NE, datum);
	}

	constexpr auto operator<(scmp_datum_t datum) const {
		return Cmp(SCMP_CMP_LT, datum);
	}

	constexpr auto operator>(scmp_datum_t datum) const {
		return Cmp(SCMP_CMP_GT, datum);
	}

	constexpr auto operator<=(scmp_datum_t datum) const {
		return Cmp(SCMP_CMP_LE, datum);
	}

	constexpr auto operator>=(scmp_datum_t datum) const {
		return Cmp(SCMP_CMP_GE, datum);
	}

	constexpr auto operator&(scmp_datum_t mask) const {
		return MaskedArg(arg, mask);
	}

private:
	/**
	 * Internal helper class.  Do not use directly.
	 */
	class MaskedArg {
		unsigned arg;
		scmp_datum_t mask;

	public:
		constexpr MaskedArg(unsigned _arg, scmp_datum_t _mask)
		:arg(_arg), mask(_mask) {}

		constexpr auto operator==(scmp_datum_t datum) const {
			return SCMP_CMP(arg, SCMP_CMP_MASKED_EQ, mask, datum);
		}
	};
};

#ifdef __clang__
#pragma GCC diagnostic pop
#endif

} // namespace Seccomp
