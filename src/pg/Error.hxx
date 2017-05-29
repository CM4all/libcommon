/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef PG_ERROR_HXX
#define PG_ERROR_HXX

#include "Result.hxx"

#include <exception>

namespace Pg {

class Error final : public std::exception {
    Result result;

public:
    Error(const Error &other) = delete;
    Error(Error &&other)
        :result(std::move(other.result)) {}
    explicit Error(Result &&_result)
        :result(std::move(_result)) {}

    Error &operator=(const Error &other) = delete;

    Error &operator=(Error &&other) {
        result = std::move(other.result);
        return *this;
    }

    Error &operator=(Result &&other) {
        result = std::move(other);
        return *this;
    }

    gcc_pure
    ExecStatusType GetStatus() const {
        return result.GetStatus();
    }

    gcc_pure
    const char *what() const noexcept override {
        return result.GetErrorMessage();
    }
};

} /* namespace Pg */

#endif
