// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "util/StringStrip.hxx"
#include "util/CharUtil.hxx"

#include <stdexcept>

class LineParser {
	char *p;

public:
	using Error = std::runtime_error;

	explicit LineParser(char *_p) noexcept
		:p(StripLeft(_p))
	{
		StripRight(p);
	}

	/**
	 * Replace the string pointer.  This is a kludge for class
	 * #VariableConfigParser.
	 */
	void Replace(char *_p) noexcept {
		p = _p;
	}

	char *Rest() noexcept {
		return p;
	}

	void Strip() noexcept {
		p = StripLeft(p);
	}

	char front() const noexcept {
		return *p;
	}

	bool IsEnd() const noexcept {
		return front() == 0;
	}

	void ExpectWhitespace();
	void ExpectEnd();
	void ExpectSymbol(char symbol);
	void ExpectSymbolAndEol(char symbol);

	bool SkipSymbol(char symbol) {
		bool found = front() == symbol;
		if (found)
			++p;
		return found;
	}

	bool SkipSymbol(char a, char b) noexcept {
		bool found = p[0] == a && p[1] == b;
		if (found)
			p += 2;
		return found;
	}

	/**
	 * If the next word matches the given parameter, then skip it and
	 * return true.  If not, the method returns false, leaving the
	 * object unmodified.
	 */
	bool SkipWord(const char *word) noexcept;

	const char *NextWord() noexcept;
	char *NextValue() noexcept;
	char *NextRelaxedValue() noexcept;
	char *NextUnescape() noexcept;

	bool NextBool();
	unsigned NextPositiveInteger();

	const char *ExpectWord();

	const char *ExpectWordAndSymbol(char symbol,
					const char *error1,
					const char *error2);

	/**
	 * Expect a non-empty value.
	 */
	char *ExpectValue();

	/**
	 * Expect a non-empty value and end-of-line.
	 */
	char *ExpectValueAndEnd();

	static constexpr bool IsWordChar(char ch) {
		return IsAlphaNumericASCII(ch) || ch == '_';
	}

private:
	char *NextUnquotedValue() noexcept;
	char *NextRelaxedUnquotedValue() noexcept;
	char *NextQuotedValue(char stop) noexcept;

	static constexpr bool IsUnquotedChar(char ch) noexcept {
		return IsWordChar(ch) || ch == '.' || ch == '-' || ch == ':';
	}

	static constexpr bool IsQuote(char ch) noexcept {
		return ch == '"' || ch == '\'';
	}
};
