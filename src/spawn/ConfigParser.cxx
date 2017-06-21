/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "ConfigParser.hxx"
#include "Config.hxx"
#include "io/FileLineParser.hxx"
#include "util/RuntimeError.hxx"

#include <pwd.h>
#include <grp.h>

static uid_t
ParseUser(const char *name)
{
    char *endptr;
    unsigned long i = strtoul(name, &endptr, 10);
    if (endptr > name && *endptr == 0)
        return i;

    const auto *pw = getpwnam(name);
    if (pw == nullptr)
        throw FormatRuntimeError("No such user: %s", name);

    return pw->pw_uid;
}

static gid_t
ParseGroup(const char *name)
{
    char *endptr;
    unsigned long i = strtoul(name, &endptr, 10);
    if (endptr > name && *endptr == 0)
        return i;

    const auto *gr = getgrnam(name);
    if (gr == nullptr)
        throw FormatRuntimeError("No such group: %s", name);

    return gr->gr_gid;
}

void
SpawnConfigParser::ParseLine(FileLineParser &line)
{
    const char *word = line.ExpectWord();

    if (strcmp(word, "allow_user") == 0) {
        config.allowed_uids.insert(ParseUser(line.ExpectValueAndEnd()));
    } else if (strcmp(word, "allow_group") == 0) {
        config.allowed_gids.insert(ParseGroup(line.ExpectValueAndEnd()));
    } else
        throw LineParser::Error("Unknown option");
}
