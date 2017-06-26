#include "util/Exception.hxx"

#include <gtest/gtest.h>

TEST(ExceptionTest, RuntimeError)
{
    ASSERT_EQ(GetFullMessage(std::make_exception_ptr(std::runtime_error("Foo"))), "Foo");
}

TEST(ExceptionTest, DerivedError)
{
    class DerivedError : public std::runtime_error {
    public:
        explicit DerivedError(const char *_msg)
            :std::runtime_error(_msg) {}
    };

    ASSERT_EQ(GetFullMessage(std::make_exception_ptr(DerivedError("Foo"))), "Foo");
}
