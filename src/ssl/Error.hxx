/*
 * OpenSSL error reporting.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef SSL_ERROR_HXX
#define SSL_ERROR_HXX

#include <stdexcept>

class SslError : public std::runtime_error {
public:
    SslError(const std::string &msg);
};

#endif
