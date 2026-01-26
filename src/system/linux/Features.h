#include <features.h> // for __GLIBC__ and __GLIBC_MINOR__

#define HAVE_OPENAT2

#ifdef __GLIBC__
#if __GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 43)
#define HAVE_NATIVE_OPENAT2
#endif
#endif
