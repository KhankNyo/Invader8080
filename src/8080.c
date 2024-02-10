
#include "Common.h"

typedef struct Intel8080 Intel8080;
typedef void (*I8080WriteFn)(Intel8080 *i8080, uint16_t Address, uint8_t Byte);
typedef uint8_t (*I8080ReadFn)(Intel8080 *i8080, uint16_t Address);
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
    I8080ReadFn PortRead;   /* called by IN instruction */
    I8080WriteFn PortWrite; /* called by OUT instruction */

    uint16_t Data;
    uint8_t Opcode;
    uint8_t Halted;
    uint8_t InterruptEnable;
};


Intel8080 I8080Init(uint16_t PC, void *UserData, 
        I8080ReadFn ReadFn, I8080WriteFn WriteFn,
        I8080ReadFn PortReadFn, I8080WriteFn PortWriteFn
);
void I8080AdvanceClock(Intel8080 *i8080);
void I8080Interrupt(Intel8080 *i8080, unsigned InterruptNumber);





/*==================================== 
 *          IMPLEMENTATION
 *====================================*/

typedef enum I8080Flags
{
    FLAG_C =    0x0001, /* upper 8 bits: position, lower 8 bits: mask */
    FLAG_P =    0x0204,
    FLAG_AC =   0x0410,
    FLAG_Z =    0x0640,
    FLAG_S =    0x0780,
} I8080Flags;



#define HL_INDIRECT 06
#define SET_FLAG(flFlag, Value)\
    i8080->Status = (i8080->Status & ~flFlag) | ((unsigned)(Value) << (flFlag >> 8))
#define GET_FLAG(flFlag) \
    (((i8080)->Status & (flFlag)) >> ((flFlag) >> 8))
/* does the arith operation has carry from bit 3 to bit 4? */
#define HAS_MIDDLE_CARRY(Ret, A, B) \
    (1 & ( ((~(Ret)&((A) | (B))) | ((A)&(B))) >> 3))
#define READ_PAIR(High, Low) \
    (((uint16_t)i8080->High << 8) | i8080->Low)
#define WRITE_PAIR(High, Low, Dat) do {\
        i8080->High = (Dat) >> 8; \
        i8080->Low = (Dat) & 0xFF;\
    } while (0)
#define READ_SRC(IntoVariable, SrcEncoding, Cyc, CycMem, CycReg) do {\
        unsigned sss_ = SrcEncoding;\
        if (sss_ == HL_INDIRECT) {\
            IntoVariable = i8080ReadByte(i8080, READ_PAIR(H, L));\
            Cyc = CycMem;\
        } else {\
            IntoVariable = i8080->R[sss_];\
            Cyc = CycReg;\
        }\
    } while (0)
#define WRITE_DST(DstEncoding, u8Data, Cyc, CycMem, CycReg) do {\
        unsigned ddd_ = DstEncoding;\
        if (ddd_ == HL_INDIRECT) {\
            i8080WriteByte(i8080, READ_PAIR(H, L), u8Data);\
            Cyc = CycMem;\
        } else {\
            i8080->R[ddd_] = u8Data;\
            Cyc = CycReg;\
        }\
    } while (0)




Intel8080 I8080Init(uint16_t PC, void *UserData, 
        I8080ReadFn ReadFn, I8080WriteFn WriteFn,
        I8080ReadFn PortReadFn, I8080WriteFn PortWriteFn)
{
    Intel8080 i8080 = {
        .PC = PC,
        .UserData = UserData,

        .Read = ReadFn,
        .Write = WriteFn,
        .PortRead = PortReadFn,
        .PortWrite = PortWriteFn,

        .InterruptEnable = true,
        .Halted = false,
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
    i8080->Data |= (uint16_t)i8080->Read(i8080, Address) << 8;
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
    switch (Index)
    {
    case 0: return READ_PAIR(B, C);
    case 1: return READ_PAIR(D, E);
    case 2: return READ_PAIR(H, L);
    case 3: return i8080->SP;
    }
    return 0;
}

static void i8080WriteRegPair(Intel8080 *i8080, unsigned Index, uint16_t Data)
{
    switch (Index)
    {
    case 0: WRITE_PAIR(B, C, Data); break;
    case 1: WRITE_PAIR(D, E, Data); break;
    case 2: WRITE_PAIR(H, L, Data); break;
    case 3: i8080->SP = Data; break;
    }
}

static void i8080ArithOp(Intel8080 *i8080, unsigned Op, unsigned Dst, unsigned Src)
{
    unsigned OriginalDst = Dst;
    switch (Op)
    {
    case 0: Dst += Src; break;
    case 1: Dst += Src + GET_FLAG(FLAG_C); break;
    case 2: Dst -= Src; break;
    case 3: Dst -= Src + GET_FLAG(FLAG_C); break;
    case 4: Dst &= Src; break;
    case 5: Dst ^= Src; break;
    case 6: Dst |= Src; break;
    case 7: Dst -= Src; goto TestFlags;
    }
    i8080->A = Dst & 0xFF;

TestFlags:
    SET_FLAG(FLAG_C, Dst > 0xFF);
    SET_FLAG(FLAG_Z, (Dst & 0xFF) == 0);
    SET_FLAG(FLAG_S, (Dst >> 7) & 1);
    SET_FLAG(FLAG_P, BitCount32(Dst & 0xFF) % 2 == 0);
    SET_FLAG(FLAG_AC, HAS_MIDDLE_CARRY(Dst, OriginalDst, Src));
}


static void i8080Push(Intel8080 *i8080, uint16_t Data)
{
    i8080->SP -= 2;
    i8080WriteWord(i8080, i8080->SP, Data);
}

static uint16_t i8080Pop(Intel8080 *i8080)
{
    uint16_t Address = i8080->SP;
    i8080->SP += 2;
    return i8080ReadWord(i8080, Address);
}

static bool i8080ConditionIsTrue(Intel8080 *i8080, unsigned Condition)
{
    enum CondName {
        COND_NZ = 0,
        COND_Z,
        COND_NC,
        COND_C,
        COND_PO,    /* parity odd */
        COND_PE,    /* parity even */
        COND_P,     /* positive (S flag is not set) */
        COND_N,     /* negative */
    };
    switch ((enum CondName)Condition)
    {
    case COND_NZ: return !GET_FLAG(FLAG_Z);
    case COND_Z:  return GET_FLAG(FLAG_Z);
    case COND_NC: return !GET_FLAG(FLAG_C);
    case COND_C:  return GET_FLAG(FLAG_C);
    case COND_PO: return !GET_FLAG(FLAG_P);
    case COND_PE: return GET_FLAG(FLAG_P);
    case COND_P:  return !GET_FLAG(FLAG_S);
    case COND_N:  return GET_FLAG(FLAG_S);
    }
    return false;
}

void I8080AdvanceClock(Intel8080 *i8080)
{
#define RP(Opc) (0x3 & (Opc >> 4))
#define DDD(Opc) (0x7 & (Opc >> 3))
#define SSS(Opc) (0x7 & (Opc))
    if (i8080->Halted)
        return;
    if (i8080->Clock)
    {
        i8080->Clock--;
        return;
    }

    uint8_t Opcode = i8080FetchOpcode(i8080);
    unsigned CycleCount = 0;
    switch (Opcode)
    {
    case 0x00: /* nop: No OPeration */
    case 0x10:
    case 0x20:
    case 0x30:
    case 0x08:
    case 0x18:
    case 0x28:
    case 0x38:
    {
        CycleCount = 4;
    } break;
    case 0x07: /* rlc: Rotate Left Carry */
    {
        unsigned Sign = i8080->A >> 7;
        i8080->A = (i8080->A << 1) | Sign;
        SET_FLAG(FLAG_C, Sign);
        CycleCount = 4;
    } break;
    case 0x0F: /* rrc: Rotate Right Carry */
    {
        unsigned First = i8080->A & 1;
        i8080->A = (i8080->A >> 1) | (First << 7);
        SET_FLAG(FLAG_C, First);
        CycleCount = 4;
    } break;
    case 0x17: /* ral: Rotate Accumulator Left */
    {
        unsigned Sign = i8080->A >> 7;
        i8080->A = (i8080->A << 1) | GET_FLAG(FLAG_C);
        SET_FLAG(FLAG_C, Sign);
        CycleCount = 4;
    } break;
    case 0x1F: /* rar: Rotate Accumulator Right */ 
    {
        unsigned First = i8080->A & 1;
        i8080->A = (i8080->A >> 1) | (GET_FLAG(FLAG_C) << 7);
        SET_FLAG(FLAG_C, First);
        CycleCount = 4;
    } break;
    case 0x27: /* daa: Decimal Adjust Accumulator */ 
    {
        /* adjust low nibble */
        unsigned LowNibble = i8080->A & 0xF;
        if (LowNibble > 9 || GET_FLAG(FLAG_AC))
        {
            LowNibble += 6; /* turn a binary carry into a decimal carry */
        }
        SET_FLAG(FLAG_AC, LowNibble >> 4);

        /* adjust high nibble */
        unsigned HighNibble = (i8080->A & 0xF0) + (LowNibble & 0xF0);
        if (HighNibble > 0x90 || GET_FLAG(FLAG_C))
        {
            HighNibble += 0x60; /* turn a binary carry into a decimal carry */
        }
        SET_FLAG(FLAG_C, HighNibble >> 8);

        /* writeback */
        i8080->A = HighNibble | (LowNibble & 0x0F);
        SET_FLAG(FLAG_S, i8080->A >> 7);
        SET_FLAG(FLAG_Z, i8080->A == 0);
        SET_FLAG(FLAG_P, BitCount32(i8080->A) % 2 == 0);

        CycleCount = 4;
    } break;
    case 0x2F: /* cma: CoMpliment Accumulator */ 
    {
        i8080->A = ~i8080->A;
        CycleCount = 4;
    } break;
    case 0x3F: /* cmc: CoMpliment Carry */ 
    {
        SET_FLAG(FLAG_C, !GET_FLAG(FLAG_C));
        CycleCount = 4;
    } break;
    case 0x37: /* stc: SeT flag C */ 
    { 
        SET_FLAG(FLAG_C, 1);
        CycleCount = 4;
    } break;
    case 0x76: /* hlt: Halt */
    {
        i8080->Halted = true;
        CycleCount = 7;
    } break;
    case 0xC9: /* ret: Return */
    case 0xD9:
    {
        i8080->PC = i8080Pop(i8080);
        CycleCount = 10;
    } break;
    case 0xE3: /* xthl: eXchange sTack with HL */
    {
        uint16_t Tmp = i8080ReadWord(i8080, i8080->SP);
        i8080WriteWord(i8080, i8080->SP, READ_PAIR(H, L));
        WRITE_PAIR(H, L, Tmp);
        CycleCount = 18;
    } break;
    case 0xE9: /* pchl: PC = HL */
    {
        i8080->PC = READ_PAIR(H, L);
        CycleCount = 5;
    } break;
    case 0xEB: /* xchg: swap(HL, DE) */
    {
        /* this does not violate strict anti-alias bc D and H are uint8's */
        /* also endian does not matter since we're just copying data */
        uint16_t *DE = (uint16_t *)&i8080->D;
        uint16_t *HL = (uint16_t *)&i8080->H;
        uint16_t Tmp = *DE;
        *DE = *HL;
        *HL = Tmp;
        CycleCount = 5;
    } break;
    case 0xF9: /* sphl: SP = HL */
    { 
        i8080->SP = READ_PAIR(H, L);
        CycleCount = 5;
    } break;
    case 0xF3: /* di: Disable Interrupt */
    {
        i8080->InterruptEnable = false;
        CycleCount = 4;
    } break;
    case 0xFB: /* ei: Enable Interrupt */
    {
        i8080->InterruptEnable = true;
        CycleCount = 4;
    } break;

    case 0xDB: /* in: load A from port */
    {
        uint8_t PortNumber = i8080FetchByte(i8080);
        i8080->A = i8080->PortRead(i8080, PortNumber);
        CycleCount = 10;
    } break;
    case 0xD3: /* out: store A to port */
    {
        uint8_t PortNumber = i8080FetchByte(i8080);
        i8080->PortWrite(i8080, PortNumber, i8080->A);
        CycleCount = 10;
    } break;

    case 0x22: /* shld: Store HL to memory */ 
    {
        uint16_t Address = i8080FetchWord(i8080);
        i8080WriteWord(i8080, Address, READ_PAIR(H, L));
        CycleCount = 16;
    } break;
    case 0x2A: /* lhld: Load HL to memory*/
    {
        uint16_t Address = i8080FetchWord(i8080);
        uint16_t Data = i8080ReadWord(i8080, Address);
        WRITE_PAIR(H, L, Data);
        CycleCount = 16;
    } break;
    case 0x32: /* sta: STore Accumulator to memory */
    {
        uint16_t Address = i8080FetchWord(i8080);
        i8080WriteByte(i8080, Address, i8080->A);
        CycleCount = 13;
    } break;
    case 0x3A: /* lda: LoaD Accumulator from memory */
    {
        uint16_t Address = i8080FetchWord(i8080);
        i8080->A = i8080ReadByte(i8080, Address);
        CycleCount = 13;
    } break;
    case 0xC3: /* jmp: PC = Addr */
    case 0xCB:
    {
        i8080->PC = i8080FetchWord(i8080);
        CycleCount = 10;
    } break;
    case 0xCD: /* call: push pc; pc = addr */
    case 0xDD:
    case 0xED:
    case 0xFD:
    {
        i8080Push(i8080, i8080->PC + 2);
        i8080->PC = i8080FetchWord(i8080);
        CycleCount = 17;
    } break;

    default:
    {
        switch (Opcode >> 6)
        {
        case 1: /* mov: MOVe source to destination */
        {
            /* NOTE: when Src == Dst == HL_INDIRECT, 
             * this will be a halt instruction, 
             * which is already handled and is unreachable in here */
            uint8_t Byte;
            READ_SRC(Byte, SSS(Opcode), 
                CycleCount, 7, 4
            );
            WRITE_DST(DDD(Opcode), Byte,
                CycleCount, 7, 4
            );
        } break;
        case 0:
        {
            switch (SSS(Opcode))
            {
            case 01: /* dad, lxi: */
            {
                unsigned RegPairIndex = RP(Opcode);
                if (Opcode & 0x08) /* dad: Double ADd */
                {
                    uint32_t Result = READ_PAIR(H, L);
                    Result += i8080ReadRegPair(i8080, RegPairIndex);
                    WRITE_PAIR(H, L, Result);
                    SET_FLAG(FLAG_C, Result > 0xFFFF);
                }
                else /* lxi: Load eXtended Immediate */
                {
                    uint16_t ImmediateWord = i8080FetchWord(i8080);
                    i8080WriteRegPair(i8080, RegPairIndex, ImmediateWord);
                }
                CycleCount = 10;
            } break;
            case 02: /* ldax, stax */
            {
                unsigned RegPairIndex = RP(Opcode);
                uint16_t Address = i8080ReadRegPair(i8080, RegPairIndex);
                if (Opcode & 0x08) /* ldax: LoaD Accumulator indirect */
                {
                    i8080->A = i8080ReadByte(i8080, Address);
                }
                else /* stax: STore Accumulator indirect */
                {
                    i8080WriteByte(i8080, Address, i8080->A);
                }
                CycleCount = 7;
            } break;
            case 03: /* inx, dcx: INcrement and DeCrement register pair */
            {
                unsigned RegPairIndex = RP(Opcode);
                uint16_t Data = i8080ReadRegPair(i8080, RegPairIndex);
                int i = (Opcode & 0x08)? 
                    -1 : 1;
                i8080WriteRegPair(i8080, RegPairIndex, Data + i);
                CycleCount = 5;
            } break;
            case 04: /* inr: INcRement */
            case 05: /* dcr: DeCRement */
            {
                unsigned Result, Data;
                int i = SSS(Opcode) == 05? 
                    -1 : 1;
                if (DDD(Opcode) == HL_INDIRECT)
                {
                    uint16_t Address = READ_PAIR(H, L);
                    Data = i8080ReadByte(i8080, Address);
                    Result = Data + i;
                    i8080WriteByte(i8080, Address, Result);
                    CycleCount = 10;
                }
                else
                {
                    Data = i8080->R[DDD(Opcode)];
                    Result = Data + i;
                    i8080->R[DDD(Opcode)] = Result;
                    CycleCount = 5;
                }

                SET_FLAG(FLAG_Z, (Result & 0xFF) == 0);
                SET_FLAG(FLAG_S, 1 & (Result >> 7));
                SET_FLAG(FLAG_P, BitCount32(Result & 0xFF) % 2 == 0);
                SET_FLAG(FLAG_AC, HAS_MIDDLE_CARRY(Result, Data, i));
            } break;
            case 06: /* mvi: MoVe Immediate */
            {
                uint8_t Byte = i8080FetchByte(i8080);
                WRITE_DST(DDD(Opcode), Byte, 
                    CycleCount, 10, 7
                );
            } break;
            }
        } break;
        case 2: /* 0b10mmmsss; m: mnemonic, s: source */
        {
            uint8_t Byte;
            READ_SRC(Byte, SSS(Opcode), 
                CycleCount, 7, 4
            );
            i8080ArithOp(i8080, DDD(Opcode), i8080->A, Byte);
        } break;
        case 3:
        {
            switch (SSS(Opcode))
            {
            case 00: /* 0b11ccc000: Rcc: Conditional Return */
            {
                if (i8080ConditionIsTrue(i8080, DDD(Opcode)))
                {
                    i8080->PC = i8080Pop(i8080);
                    CycleCount = 11;
                }
                CycleCount = 5;
            } break;
            case 01: /* 0b11rp0001: Pop */
            {
                unsigned RegPairIndex = RP(Opcode);
                if (0x3 == RegPairIndex) /* PSW */
                {
                    uint16_t ProcessorStatusWord = i8080Pop(i8080);
                    i8080->A = ProcessorStatusWord >> 8;
                    i8080->Status = ProcessorStatusWord & 0xFF;
                }
                else
                {
                    uint16_t Data = i8080Pop(i8080);
                    i8080WriteRegPair(i8080, RegPairIndex, Data);
                }
                CycleCount = 10;
            } break;
            case 02: /* 0b11ccc010: Jcc */
            {
                uint16_t Address = i8080FetchWord(i8080);
                if (i8080ConditionIsTrue(i8080, DDD(Opcode)))
                {
                    i8080->PC = Address;
                }
                CycleCount = 10;
            } break;
            case 04: /* 0b11ccc100: Ccc */
            {
                uint16_t Address = i8080FetchWord(i8080);
                if (i8080ConditionIsTrue(i8080, DDD(Opcode)))
                {
                    i8080Push(i8080, i8080->PC);
                    i8080->PC = Address;
                    CycleCount = 17;
                }
                else
                {
                    CycleCount = 11;
                }
            } break;
            case 05: /* 0b11RP_0101: PUSH RP; PSW */
            {
                unsigned RegPairIndex = RP(Opcode);
                if (0x3 == RegPairIndex) /* PSW */
                {
                    uint16_t ProgramStatusWord = 
                        ((uint16_t)i8080->A << 8)
                        | (i8080->Status & 0xFF);
                    i8080Push(i8080, ProgramStatusWord);
                }
                else
                {
                    uint16_t Data = i8080ReadRegPair(i8080, RegPairIndex);
                    i8080Push(i8080, Data);
                }
                CycleCount = 11;
            } break;
            case 06: /* 0b11ccc110: c: opcode for arith op */
            {
                uint8_t Byte = i8080FetchByte(i8080);
                i8080ArithOp(i8080, DDD(Opcode), i8080->A, Byte);
                CycleCount = 7;
            } break;
            case 07: /* 0b11nnn111: RST n */
            {
                uint8_t InterruptVector = Opcode & 070;
                i8080Push(i8080, i8080->PC);
                i8080->PC = InterruptVector;
                CycleCount = 11;
            } break;
            }
        } break;
        }
    } break;
    }
    i8080->Clock = CycleCount;

#undef SSS
#undef DDD
#undef RP
}

void I8080Interrupt(Intel8080 *i8080, unsigned InterruptNumber)
{
    if (!i8080->InterruptEnable)
        return;

    i8080->Halted = false;
    i8080->InterruptEnable = false;

    /* rst instruction */
    i8080Push(i8080, i8080->PC);
    uint16_t Vector = (InterruptNumber & 0x7)*8;
    i8080->PC = Vector;
    i8080->Clock = 11;
}

#undef HL_INDIRECT
#undef SET_FLAG
#undef GET_FLAG
#undef READ_PAIR
#undef WRITE_PAIR
#undef READ_SRC
#undef WRITE_DST
#undef HAS_MIDDLE_CARRY




#ifdef STANDALONE
#undef STANDALONE
#include "Disassembler.c"
#include "File.c"

#include <stdio.h>

static void EmuPrintUsage(const char *ProgramName)
{
    printf("Usage: %s <tst8080.bin file>\n", ProgramName);
}



static uint8_t sBuffer[1 << 16];

static uint8_t CPMReadFn(Intel8080 *i8080, uint16_t Address)
{
    if (0x0005 == Address) /* CPM string output */
    {
        switch (i8080->C)
        {
        case 9: /* string output in DE */
        {
            uint16_t Index = ((uint16_t)i8080->D << 8) | i8080->E;
            const char *String = (const char *)&sBuffer[Index];
            int i = 0; 
            while (String[i] != '$')
            {
                fputc(String[i], stdout);
                i++;
            }
        } break;
        case 2: /* char output in E */
        {
            fputc(i8080->E, stdout);
        } break;
        }
    } 
    else if (0x0000 == Address)
    {
        /* warm boot, that means we passed all tst8080 tests */
        exit(0);
    }
    else if (Address + 1u > sizeof sBuffer)
    {
        printf("Warning: invalid read at %04x\n", Address);
        return 0;
    }
    return sBuffer[Address];
}

static void CPMWriteFn(Intel8080 *i8080, uint16_t Address, uint8_t Byte)
{
    if (Address + 1u > sizeof sBuffer)
    {
        printf("Warning: invalid write at %04x\n", Address);
        return;
    }
    sBuffer[Address] = Byte;
}


static char GetInput(void)
{
    static char Buffer[256];
    fgets(Buffer, sizeof Buffer, stdin);
    return Buffer[0];
}

static void DumpStatus(const Intel8080 *i8080)
{
    char Line[LINE_LEN];
    const uint8_t *CurrentInstruction = &sBuffer[i8080->PC];
    DisassembleInstructionIntoString(Line, true, 
        CurrentInstruction, CurrentInstruction + 3
    );

    char Flag[16] = "sz_a_p_c";
    for (int i = 0; i < 8; i++)
    {
        if (i8080->Status & (1 << (7 - i)))
        {
            /* to upper */
            if (Flag[i] == '_')
            {
                Flag[i] = '1';
            }
            else Flag[i] &= ~(1 << 5);
        }
    }


    uint16_t StackTop = sBuffer[i8080->SP] | 
        ((uint16_t)sBuffer[i8080->SP + 1] << 8);
    printf(
        "A[%02x]|S[%02x] = %s\n"
        "B[%02x]|C[%02x]|\n"
        "D[%02x]|E[%02x]|\n"
        "H[%02x]|L[%02x]|\n"
        "Data: %04x\n"
        "[SP=%04x]: %04x\n"
        "[PC=%04x]: %s\n", 
        i8080->A, i8080->Status, Flag,
        i8080->B, i8080->C,
        i8080->D, i8080->E,
        i8080->H, i8080->L, 
        i8080->Data,
        i8080->SP, StackTop, 
        i8080->PC, Line
    );
}


int main(int argc, char **argv)
{
    if (argc < 2)
    {
        EmuPrintUsage(argv[0]);
        return 0;
    }
    const char *InputFile = argv[1];
    if (!ReadFileIntoBuffer(&sBuffer[0x100], sizeof sBuffer - 0x100, InputFile))
    {
        perror(InputFile);
        return 1;
    }

    sBuffer[0x0005] = 0xC9; /* return instruction for string output addr */
    Intel8080 i8080 = I8080Init(
        0x100, NULL, 
        CPMReadFn, CPMWriteFn,
        CPMReadFn, CPMWriteFn
    );
#if 0
    /* single step */
    int ShouldQuit = 0;
    while (!ShouldQuit)
    {
        if (i8080.PC >= 0x0690)
        {
            DumpStatus(&i8080);
            ShouldQuit = GetInput() == 'q';
        }
        I8080AdvanceClock(&i8080);
    }
#else
    /* run until warm boot */
    while (1)
    {
        I8080AdvanceClock(&i8080);
    }
#endif 
    return 0;
}
#endif /* STANDALONE */



