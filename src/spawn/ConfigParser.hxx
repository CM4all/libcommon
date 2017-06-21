/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef SPAWN_CONFIG_PARSER_HXX
#define SPAWN_CONFIG_PARSER_HXX

#include "io/ConfigParser.hxx"

struct SpawnConfig;

class SpawnConfigParser final : public ConfigParser {
    SpawnConfig &config;

public:
    explicit SpawnConfigParser(SpawnConfig &_config):config(_config) {}

protected:
    /* virtual methods from class ConfigParser */
    void ParseLine(FileLineParser &line) override;
};

#endif
