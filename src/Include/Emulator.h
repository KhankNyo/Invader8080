#ifndef EMULATOR_H
#define EMULATOR_H


#include "Common.h"

typedef struct Intel8080 Intel8080;
typedef void (*I8080WriteFn)(Intel8080 *i8080, uint16_t Address, uint8_t Byte);
typedef uint8_t (*I8080ReadFn)(Intel8080 *i8080, uint16_t Address);

typedef enum I8080Flags
{
    FLAG_C =    0x0001, /* upper 8 bits: position, lower 8 bits: mask */
    FLAG_P =    0x0204,
    FLAG_AC =   0x0410,
    FLAG_Z =    0x0640,
    FLAG_S =    0x0780,
} I8080Flags;

struct Intel8080 
{
    union {
        struct {
            uint8_t B, C, 
                    D, E, 
                    H, L, 
                    M,  /* NOTE: M is not a real register, it's a place holder for mode 110, 
                           which is a memory reference through HL */
                    A;
        };
        uint8_t R[8];
    };
    uint8_t Status;
    uint8_t Clock;
    uint16_t PC;
    uint16_t SP;
    void *UserData;
    I8080ReadFn Read;
    I8080WriteFn Write;

    uint8_t Opcode;
    uint16_t Data;
};


Intel8080 I8080Init(uint16_t PC, void *UserData, I8080ReadFn ReadFn, I8080WriteFn WriteFn);
void I8080AdvanceClock(Intel8080 *i8080);

#endif /* EMULATOR_H */

