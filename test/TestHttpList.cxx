#include "../src/http/List.hxx"

#include <assert.h>

int
main(int, char **)
{
    assert(http_list_contains("foo", "foo"));
    assert(!http_list_contains("foo", "bar"));
    assert(http_list_contains("foo,bar", "bar"));
    assert(http_list_contains("bar,foo", "bar"));
    assert(!http_list_contains("bar,foo", "bart"));
    assert(!http_list_contains("foo,bar", "bart"));
    assert(http_list_contains("bar,foo", "\"bar\""));
    assert(http_list_contains("\"bar\",\"foo\"", "\"bar\""));
    assert(http_list_contains("\"bar\",\"foo\"", "bar"));
    return 0;
}
