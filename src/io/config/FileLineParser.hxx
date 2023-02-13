// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "LineParser.hxx"

#include <filesystem>

class FileLineParser : public LineParser {
	const std::filesystem::path &base_path;

public:
	FileLineParser(const std::filesystem::path &_base_path, char *_p)
		:LineParser(_p), base_path(_base_path) {}

	std::filesystem::path ExpectPath();
	std::filesystem::path ExpectPathAndEnd();
};
