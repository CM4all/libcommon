#include "../src/pg/Array.hxx"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static void
check_encode(const std::list<std::string> &input,
             const char *expected)
{
    std::string result = pg_encode_array(input);

    if (strcmp(result.c_str(), expected) != 0) {
        fprintf(stderr, "got '%s', expected '%s'\n",
                result.c_str(), expected);
        exit(2);
    }
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    std::list<std::string> a;
    check_encode(a, "{}");

    a.push_back("foo");
    check_encode(a, "{\"foo\"}");

    a.push_back("");
    check_encode(a, "{\"foo\",\"\"}");

    a.push_back("\\");
    check_encode(a, "{\"foo\",\"\",\"\\\\\"}");

    a.push_back("\"");
    check_encode(a, "{\"foo\",\"\",\"\\\\\",\"\\\"\"}");

    return 0;
}
