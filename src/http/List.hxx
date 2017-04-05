/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef HTTP_LIST_HXX
#define HTTP_LIST_HXX

#include <inline/compiler.h>

gcc_pure
bool
http_list_contains(const char *list, const char *item);

/**
 * Case-insensitive version of http_list_contains().
 */
gcc_pure
bool
http_list_contains_i(const char *list, const char *item);

#endif
