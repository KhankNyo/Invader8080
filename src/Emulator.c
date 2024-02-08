
#include "Emulator.h"

#define SET_FLAG(pI8080, flFlag, Value)\
    (pI8080)->Status = ((pI8080)->Status & ~(flFlag)) /* zero out the flag slot */\
                    | ((0xFF & flFlag) & ((Value) << (flFlag >> 8))) /* logical or the new value in */
#define GET_FLAG(pI8080, flFlag)\
    (((p8080)->Status & (flFlag)) >> ((flFlag) >> 8))


Intel8080 I8080Init(uint16_t PC, void *UserData, I8080ReadFn ReadFn, I8080WriteFn WriteFn)
{
    Intel8080 i8080 = {
        .PC = PC,
        .UserData = UserData,
        .Read = ReadFn,
        .Write = WriteFn,
    };
    return i8080;
}



static void i8080WriteByte(Intel8080 *i8080, uint16_t Address, uint8_t Byte)
{
    i8080->Data = Byte;
    i8080->Write(i8080, Address, Byte);
}

static void i8080WriteWord(Intel8080 *i8080, uint16_t Address, uint16_t Data)
{
    i8080->Data = Data;
    i8080->Write(i8080, Address++, Data & 0xFF);
    i8080->Write(i8080, Address, Data >> 8);
}

static uint8_t i8080ReadByte(Intel8080 *i8080, uint16_t Address)
{
    return i8080->Data = i8080->Read(i8080, Address);
}

static uint8_t i8080FetchOpcode(Intel8080 *i8080)
{
    return i8080->Opcode = i8080->Read(i8080, i8080->PC++);
}

static uint8_t i8080FetchByte(Intel8080 *i8080)
{
    return i8080ReadByte(i8080, i8080->PC++);
}

static uint16_t i8080ReadWord(Intel8080 *i8080, uint16_t Address)
{
    i8080->Data = i8080->Read(i8080, Address++);
    i8080->Data |= i8080->Read(i8080, Address);
    return i8080->Data;
}

static uint16_t i8080FetchWord(Intel8080 *i8080)
{
    uint16_t Word = i8080ReadWord(i8080, i8080->PC);
    i8080->PC += 2;
    return Word;
}

static uint16_t i8080ReadRegPair(Intel8080 *i8080, unsigned Index)
{
#define READ(High, Low) ((uint16_t)i8080->High << 8) | i8080->Low
    switch (Index)
    {
    case 0: return READ(B, C);
    case 1: return READ(D, E);
    case 2: return READ(H, L);
    case 3: return i8080->SP;
    }
    return 0;
#undef READ
}

static void i8080WriteRegPair(Intel8080 *i8080, unsigned Index, uint16_t Data)
{
#define WRITE(High, Low, Dat) i8080->High = Dat & 0xFF; i8080->Low = Dat >> 8;
    switch (Index)
    {
    case 0: WRITE(B, C, Data); break;
    case 1: WRITE(D, E, Data); break;
    case 2: WRITE(H, L, Data); break;
    case 3: i8080->SP = Data; break;
    }
#undef WRITE 
}


void I8080AdvanceClock(Intel8080 *i8080)
{
#define RP(Opc) (0x3 & (Opc >> 4))
#define DDD(Opc) (0x7 & (Opc >> 3))
#define SSS(Opc) (0x7 & (Opc))
    if (i8080->Clock)
        i8080->Clock--;

    uint8_t Opcode = i8080FetchOpcode(i8080);
    switch (Opcode >> 6)
    {
    case 0:
    {
        /* 0b00___xxx */
        switch (0x7 & Opcode)
        {
        case 0:
        case 1:
        case 2: /* load/store */
        {
            unsigned RegPairIndex = RP(Opcode);
            if (Opcode & 0x08) /* load */
            {
                switch (RegPairIndex)
                {
                default: /* RP is reg indirect */
                {
                    uint16_t Address = i8080ReadRegPair(i8080, RegPairIndex);
                    i8080->A = i8080ReadByte(i8080, Address);
                } break;
                case 2: /* read abs into hl */
                {
                    uint16_t Address = i8080FetchWord(i8080);
                    uint16_t Data = i8080ReadWord(i8080, Address);
                    i8080->H = Data >> 8;
                    i8080->L = Data & 0xFF;
                } break;
                case 3: /* read abs into A */
                {
                    uint16_t Address = i8080FetchWord(i8080);
                    i8080->A = i8080ReadByte(i8080, Address);
                } break;
                }
            }
            else /* store */
            {
                switch (RegPairIndex)
                {
                default: /* RP is reg indirect */
                {
                    uint16_t Address = i8080ReadRegPair(i8080, RegPairIndex);
                    i8080WriteByte(i8080, Address, i8080->A);
                } break;
                case 2: /* store hl abs */
                {
                    uint16_t Address = i8080FetchWord(i8080);
                    uint16_t Word = ((uint16_t)i8080->H << 8) | i8080->L;
                    i8080WriteWord(i8080, Address, Word);
                } break;
                case 3: /* store a abs */
                {
                    uint16_t Address = i8080FetchWord(i8080);
                    i8080WriteByte(i8080, Address, i8080->A);
                } break;
                }
            }
        } break;
        case 3: /* 0b00RPx011: inx, dcx */
        {
            int Constant = Opcode & 0x08? 
                -1 : 1;
            unsigned RegPairIndex = RP(Opcode);
            uint16_t Data = i8080ReadRegPair(i8080, RegPairIndex) + Constant;
            i8080WriteRegPair(i8080, RegPairIndex, Data);
            /* does not update flags */
        } break;
        case 4: /* 0b00ddd100: inr */
        {
            uint8_t Data = ++i8080->R[DDD(Opcode)];
            SET_FLAG(i8080, FLAG_Z, Data == 0);
            SET_FLAG(i8080, FLAG_P, (Data % 2) == 0);
            SET_FLAG(i8080, FLAG_S, Data >> 7);
        } break;
        case 5: /* 0b00ddd100: dcr */
        {
            uint8_t Data = --i8080->R[DDD(Opcode)];
            SET_FLAG(i8080, FLAG_Z, Data == 0);
            SET_FLAG(i8080, FLAG_P, (Data % 2) == 0);
            SET_FLAG(i8080, FLAG_S, Data >> 7);
        } break;
        case 6: /* 0b00ddd110: mvi */
        {
            i8080->R[DDD(Opcode)] = i8080FetchByte(i8080);
        } break;
        case 7:
        {
        } break;
        }
    } break;
    case 1: /* mov d, s */
    {
        i8080->R[DDD(Opcode)] = i8080->R[SSS(Opcode)];
    } break;
    case 2:
    case 3:
    }
#undef SSS
#undef DDD
#undef RP
}



#ifdef STANDALONE
int main(void)
{
    return 0;
}
#endif /* STANDALONE */

