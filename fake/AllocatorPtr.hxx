/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef ALLOCATOR_PTR_HXX
#define ALLOCATOR_PTR_HXX

#include "util/ConstBuffer.hxx"
#include "util/StringView.hxx"

#include <forward_list>
#include <functional>
#include <new>

#include <stdlib.h>
#include <string.h>

class Allocator {
    std::forward_list<std::function<void()>> cleanup;

public:
    ~Allocator() {
        for (auto &i : cleanup)
            i();
    }

    void *Allocate(size_t size) {
        void *p = malloc(size);
        if (p == nullptr)
            throw std::bad_alloc();

        cleanup.emplace_front([p](){ free(p); });
        return p;
    }

    char *Dup(const char *src) {
        char *p = strdup(src);
        if (p == nullptr)
            throw std::bad_alloc();

        cleanup.emplace_front([p](){ free(p); });
        return p;
    }

    template<typename... Args>
    char *Concat(Args&&... args) {
        const size_t length = ConcatLength(args...);
        char *result = NewArray<char>(length + 1);
        *ConcatCopy(result, args...) = 0;
        return result;
    }

    template<typename T, typename... Args>
    T *New(Args&&... args) {
        auto p = new T(std::forward<Args>(args)...);
        cleanup.emplace_front([p](){ delete p; });
        return p;
    }

    template<typename T>
    T *NewArray(size_t n) {
        auto p = new T[n];
        cleanup.emplace_front([p](){ delete[] p; });
        return p;
    }

private:
    template<typename... Args>
    static size_t ConcatLength(const char *s, Args... args) {
        return strlen(s) + ConcatLength(args...);
    }

    template<typename... Args>
    static constexpr size_t ConcatLength(StringView s, Args... args) {
        return s.size + ConcatLength(args...);
    }

    static constexpr size_t ConcatLength() {
        return 0;
    }

    template<typename... Args>
    static char *ConcatCopy(char *p, const char *s, Args... args) {
        return ConcatCopy(stpcpy(p, s), args...);
    }

    template<typename... Args>
    static char *ConcatCopy(char *p, StringView s, Args... args) {
        return ConcatCopy((char *)mempcpy(p, s.data, s.size), args...);
    }

    template<typename... Args>
    static char *ConcatCopy(char *p) {
        return p;
    }
};

class AllocatorPtr {
    Allocator &allocator;

public:
    AllocatorPtr(Allocator &_allocator):allocator(_allocator) {}

    const char *Dup(const char *src) {
        return allocator.Dup(src);
    }

    const char *CheckDup(const char *src) {
        return src != nullptr ? allocator.Dup(src) : nullptr;
    }

    template<typename... Args>
    char *Concat(Args&&... args) {
        return allocator.Concat(std::forward<Args>(args)...);
    }

    template<typename T, typename... Args>
    T *New(Args&&... args) {
        return allocator.New<T>(std::forward<Args>(args)...);
    }

    template<typename T>
    T *NewArray(size_t n) {
        return allocator.NewArray<T>(n);
    }

    void *Dup(const void *data, size_t size) {
        auto p = allocator.Allocate(size);
        memcpy(p, data, size);
        return p;
    }

    ConstBuffer<void> Dup(ConstBuffer<void> src);

    template<typename T>
    ConstBuffer<T> Dup(ConstBuffer<T> src) {
        return ConstBuffer<T>::FromVoid(Dup(src.ToVoid()));
    }

    StringView Dup(StringView src);
    const char *DupZ(StringView src);
};

#endif
