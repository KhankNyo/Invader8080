#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define IN_RANGE(Lower, n, Upper) ((Lower) <= (n) && (n) <= (Upper))
#define STATIC_ARRAY_SIZE(Arr) (sizeof(Arr)/sizeof((Arr)[0]))
#define NONNULL(Ptr) if (NULL == Ptr) UNREACHABLE("Null pointer")
#define DIE() abort()

#define strfy(x) #x
#define STRFY(x) strfy(x)

#ifdef DEBUG 
#  include <stdio.h>
#  define UNREACHABLE(...) do {\
    fprintf(stderr, "Unreachable code path on line "STRFY(__LINE__)": " __VA_ARGS__);\
    DIE();\
} while (0)
#  define DEBUG_ASSERT(x) if (!(x)) UNREACHABLE("Assertion failed: " # x)

#else
#  define UNREACHABLE(...)  do {} while (0)
#  define DEBUG_ASSERT(x)   do {} while (0)
#endif /* DEBUG */

#define ABS(v) ((v) < 0? -(v) : (v))

static inline unsigned BitCount32(uint32_t n)
{
    unsigned i = 0;
    while (n)
    {
        n &= n - 1;
        i++;
    }
    return i;
}




#endif /* COMMON_H */

