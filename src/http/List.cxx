/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "List.hxx"
#include "util/StringView.hxx"

static StringView
http_trim(StringView s)
{
    /* trim whitespace */

    s.Strip();

    /* remove quotes from quoted-string */

    if (s.size >= 2 && s.front() == '"' && s.back() == '"') {
        s.pop_front();
        s.pop_back();
    }

    /* return */

    return s;
}

static bool
http_equals(StringView a, StringView b)
{
    return http_trim(a).Equals(http_trim(b));
}

bool
http_list_contains(const char *list, const char *_item)
{
    const StringView item(_item);

    while (*list != 0) {
        /* XXX what if the comma is within an quoted-string? */
        const char *comma = strchr(list, ',');
        if (comma == nullptr)
            return http_equals(list, item);

        if (http_equals({list, comma}, item))
            return true;

        list = comma + 1;
    }

    return false;
}

static bool
http_equals_i(StringView a, StringView b)
{
    return http_trim(a).EqualsIgnoreCase(b);
}

bool
http_list_contains_i(const char *list, const char *_item)
{
    const StringView item(_item);

    while (*list != 0) {
        /* XXX what if the comma is within an quoted-string? */
        const char *comma = strchr(list, ',');
        if (comma == nullptr)
            return http_equals_i(list, item);

        if (http_equals_i({list, comma}, item))
            return true;

        list = comma + 1;
    }

    return false;
}
