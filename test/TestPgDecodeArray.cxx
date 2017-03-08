#include "../src/pg/Array.hxx"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static void check_decode(const char *input, const char *const* expected) {
    std::list<std::string> a = pg_decode_array(input);

    unsigned i = 0;
    for (const auto &v : a) {
        if (expected[i] == NULL) {
            fprintf(stderr, "decode '%s': too many elements in result ('%s')\n",
                    input, v.c_str());
            exit(2);
        }

        if (strcmp(v.c_str(), expected[i]) != 0) {
            fprintf(stderr, "decode '%s': element %u differs: '%s', but '%s' expected\n",
                    input, i, v.c_str(), expected[i]);
            exit(2);
        }

        ++i;
    }

    if (expected[a.size()] != NULL) {
        fprintf(stderr, "decode '%s': not enough elements in result ('%s')\n",
                input, expected[a.size()]);
        exit(2);
    }
}

int main(int argc, char **argv) {
    const char *zero[] = {NULL};
    const char *empty[] = {"", NULL};
    const char *one[] = {"foo", NULL};
    const char *two[] = {"foo", "bar", NULL};
    const char *three[] = {"foo", "", "bar", NULL};
    const char *special[] = {"foo", "\"\\", NULL};

    (void)argc;
    (void)argv;

    check_decode("{}", zero);
    check_decode("{\"\"}", empty);
    check_decode("{foo}", one);
    check_decode("{\"foo\"}", one);
    check_decode("{foo,bar}", two);
    check_decode("{foo,\"bar\"}", two);
    check_decode("{foo,,bar}", three);
    check_decode("{foo,\"\\\"\\\\\"}", special);

    return 0;
}
