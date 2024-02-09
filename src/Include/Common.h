#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define IN_RANGE(Lower, n, Upper) ((Lower) <= (n) && (n) <= (Upper))
#define DIE() abort()

#ifdef DEBUG 
#  include <stdio.h>
#  define UNREACHABLE(...) do {\
    fprintf(stderr, "Unreachable code path: " __VA_ARGS__);\
    DIE();\
} while (0)
#else
#  define UNREACHABLE(...) 
#endif /* DEBUG */



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

