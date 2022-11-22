/*
 * Copyright 2022 CM4all GmbH
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

#pragma once

#include "ConfigParser.hxx"

#include <memory>

/**
 * Parser properties within an INI section.  Derive from this class
 * and return instances from IniFileParser::Section().
 */
class IniSectionParser {
public:
	virtual void Property(std::string_view name,
			      FileLineParser &value) = 0;

	/**
	 * Called when the section ends (the file ends or a new
	 * section starts).
	 *
	 * May throw on error (e.g. if the section is incomplete).
	 */
	virtual void Finish() {}
};

/**
 * Parse INI files.  Override Section() and derive section-specific
 * classes from #IniSectionParser.
 */
class IniFileParser : public ConfigParser {
	std::unique_ptr<IniSectionParser> child;

public:
	/**
	 * A section header was found.  This method may decide to
	 * continue parsing the section by returning an
	 * #IniSectionParser instance, ignore the section by returning
	 * nullptr, or throw an error.
	 */
	[[nodiscard]]
	virtual std::unique_ptr<IniSectionParser> Section(std::string_view name) = 0;

	/* virtual methods from ConfigParser */
	virtual void ParseLine(FileLineParser &line) override;
	virtual void Finish() override;
};
