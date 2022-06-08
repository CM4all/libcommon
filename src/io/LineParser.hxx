/*
 * Copyright 2007-2022 CM4all GmbH
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

#ifndef LINE_PARSER_HXX
#define LINE_PARSER_HXX

#include "util/StringStrip.hxx"
#include "util/CharUtil.hxx"

#include <stdexcept>
#include <string>

class LineParser {
	char *p;

public:
	using Error = std::runtime_error;

	explicit LineParser(char *_p):p(StripLeft(_p)) {
		StripRight(p);
	}

	/**
	 * Replace the string pointer.  This is a kludge for class
	 * #VariableConfigParser.
	 */
	void Replace(char *_p) {
		p = _p;
	}

	char *Rest() {
		return p;
	}

	void Strip() {
		p = StripLeft(p);
	}

	char front() const {
		return *p;
	}

	bool IsEnd() const {
		return front() == 0;
	}

	void ExpectWhitespace() {
		if (!IsWhitespaceNotNull(front()))
			throw std::runtime_error("Syntax error");

		++p;
		Strip();
	}

	void ExpectEnd() {
		if (!IsEnd())
			throw Error(std::string("Unexpected tokens at end of line: ") + p);
	}

	void ExpectSymbol(char symbol) {
		if (front() != symbol)
			throw Error(std::string("'") + symbol + "' expected");

		++p;
		Strip();
	}

	void ExpectSymbolAndEol(char symbol) {
		ExpectSymbol(symbol);

		if (!IsEnd())
			throw Error(std::string("Unexpected tokens after '")
				    + symbol + "': " + p);
	}

	bool SkipSymbol(char symbol) {
		bool found = front() == symbol;
		if (found)
			++p;
		return found;
	}

	bool SkipSymbol(char a, char b) {
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
	bool SkipWord(const char *word);

	const char *NextWord();
	char *NextValue();
	char *NextRelaxedValue();
	char *NextUnescape();

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
	char *NextUnquotedValue();
	char *NextRelaxedUnquotedValue();
	char *NextQuotedValue(char stop);

	static constexpr bool IsUnquotedChar(char ch) {
		return IsWordChar(ch) || ch == '.' || ch == '-' || ch == ':';
	}

	static constexpr bool IsQuote(char ch) {
		return ch == '"' || ch == '\'';
	}
};

#endif
