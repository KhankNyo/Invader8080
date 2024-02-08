
#include <stdint.h>
#include <string.h>
#include <stdio.h>


#define LINE_LEN 256
#define IN_RANGE(Lower, n, Upper) ((Lower) <= (n) && (n) <= (Upper))
typedef enum Boolean 
{
    false = 0, true
} Boolean32;

static void DisPrintUsage(const char *ProgName)
{
    printf("Usage: %s <binary file>\n", ProgName);
}

static void DisassembleSingle(char Line[LINE_LEN], uint8_t Opcode, const char *Mnemonic)
{
    sprintf(Line, "%02x         %s", Opcode, Mnemonic);
}

static const uint8_t *DisassembleAddr(char Line[LINE_LEN], uint8_t Opcode, 
    const char *Mnemonic, const uint8_t *Immediate, const uint8_t *End)
{
    if (Immediate + 2 > End)
    {
        sprintf(Line, "%02x ?? ??   %-5s ????", Opcode, Mnemonic);
        return End;
    }
    else
    {
        uint16_t Addr = *Immediate++;
        Addr |= (uint16_t)*Immediate++ << 8;
        sprintf(Line, "%02x %02x %02x   %-5s %04x", 
            Opcode, Addr & 0xFF, Addr >> 8, 
            Mnemonic, Addr
        );
        return Immediate;
    }
}

static const uint8_t *DisassembleImmByte(char Line[LINE_LEN], uint8_t Opcode, 
    const char *Mnemonic, const uint8_t *Immediate, const uint8_t *End)
{
    if (Immediate + 1 > End)
    {
        sprintf(Line, "%02x ??      %-5s ??", Opcode, Mnemonic);
        return End;
    }
    else
    {
        uint8_t Byte = *Immediate++;
        sprintf(Line, "%02x %02x      %-5s #%02x", 
            Opcode, Byte, 
            Mnemonic, Byte
        );
        return Immediate;
    }
}

static void DisassembleWithOperand(char Line[LINE_LEN], uint8_t Opcode, 
    const char *Mnemonic, const char *Operand, int OperandLength)
{
    snprintf(Line, LINE_LEN, "%02x         %-5s %.*s", Opcode, 
        Mnemonic, OperandLength, Operand
    );
}

static void PrintUnknownInstruction(char Line[LINE_LEN], uint8_t Opcode)
{
    sprintf(Line, "%02x         ???", Opcode);
}

static const uint8_t *DisassembleInstruction(FILE *OutStream, Boolean32 Capitalized, const uint8_t *Start, const uint8_t *End)
{
    if (Start + 1 > End)
        return End;

    uint8_t Opcode = *Start++;
    const uint8_t *Next = Start;
    char Line[LINE_LEN];
    switch (Opcode)
    {
    case 0x00: DisassembleSingle(Line, Opcode, "nop"); break;
    case 0x07: DisassembleSingle(Line, Opcode, "rlc"); break;
    case 0x0F: DisassembleSingle(Line, Opcode, "rrc"); break;
    case 0x17: DisassembleSingle(Line, Opcode, "ral"); break;
    case 0x1F: DisassembleSingle(Line, Opcode, "rar"); break;
    case 0x27: DisassembleSingle(Line, Opcode, "daa"); break;
    case 0x2F: DisassembleSingle(Line, Opcode, "cma"); break;
    case 0x3F: DisassembleSingle(Line, Opcode, "cmc"); break;
    case 0x37: DisassembleSingle(Line, Opcode, "stc"); break;
    case 0x76: DisassembleSingle(Line, Opcode, "hlt"); break;
    case 0xC9: DisassembleSingle(Line, Opcode, "ret"); break;
    case 0xE3: DisassembleSingle(Line, Opcode, "xthl"); break;
    case 0xE9: DisassembleSingle(Line, Opcode, "pchl"); break;
    case 0xEB: DisassembleSingle(Line, Opcode, "xchg"); break;
    case 0xF9: DisassembleSingle(Line, Opcode, "sphl"); break;
    case 0xF3: DisassembleSingle(Line, Opcode, "di"); break;
    case 0xFB: DisassembleSingle(Line, Opcode, "ei"); break;

    case 0xC6: Next = DisassembleImmByte(Line, Opcode, "adi", Next, End); break;
    case 0xCE: Next = DisassembleImmByte(Line, Opcode, "aci", Next, End); break;
    case 0xD6: Next = DisassembleImmByte(Line, Opcode, "sui", Next, End); break;
    case 0xDB: Next = DisassembleImmByte(Line, Opcode, "in", Next, End); break;
    case 0xD3: Next = DisassembleImmByte(Line, Opcode, "out", Next, End); break;
    case 0xDE: Next = DisassembleImmByte(Line, Opcode, "sbi", Next, End); break;
    case 0xE6: Next = DisassembleImmByte(Line, Opcode, "ani", Next, End); break;
    case 0xEE: Next = DisassembleImmByte(Line, Opcode, "xri", Next, End); break;
    case 0xF6: Next = DisassembleImmByte(Line, Opcode, "ori", Next, End); break;
    case 0xFE: Next = DisassembleImmByte(Line, Opcode, "cpi", Next, End); break;

    case 0x22: Next = DisassembleAddr(Line, Opcode, "shld", Next, End); break;
    case 0x2A: Next = DisassembleAddr(Line, Opcode, "lhld", Next, End); break;
    case 0x32: Next = DisassembleAddr(Line, Opcode, "sta", Next, End); break;
    case 0x3A: Next = DisassembleAddr(Line, Opcode, "lda", Next, End); break;
    case 0xC3: Next = DisassembleAddr(Line, Opcode, "jmp", Next, End); break;
    case 0xCD: Next = DisassembleAddr(Line, Opcode, "call", Next, End); break;

    default:
    {
        static const char RegisterName[] = "abcdehlm";
        static const char ConditionName[][4] = {
            "nz", "z", "nc", "c",
            "po", "pe", "p", "m"
        };
        static const char RegisterPairName[][4] = {
            "bc", "de", "hl", "sp"
        };

        switch (Opcode >> 6)
        {
        case 1:
        {
            char Dst = RegisterName[0x7 & (Opcode >> 3)];
            char Src = RegisterName[0x7 & Opcode];
            sprintf(Line, "%02x         mov   %c, %c", Opcode, Dst, Src);
        } break;
        case 0:
        {
            switch (Opcode & 0x7)
            {
            case 01:
            {
                const char *PairName = RegisterPairName[0x3 & (Opcode >> 4)];
                if (Opcode & 0x08) /* 0b00RP_1001 */
                {
                    DisassembleWithOperand(Line, Opcode, "dad", PairName, 2);
                }
                else /* 0b00RP_0001 */
                {
                    if (Next + 2 > End)
                    {
                        sprintf(Line, "%02x ?? ??   lxi   %s, #????", Opcode, PairName);
                        Next = End;
                    }
                    else
                    {
                        uint16_t Imm16 = *Next++;
                        Imm16 |= *Next++;
                        sprintf(Line, "%02x %02x %02x   lxi   %s, #%04x", 
                            Opcode, Imm16 & 0xFF, Imm16 >> 8,
                            PairName, Imm16
                        );
                    }
                }
            } break;
            case 02:
            {
                const char *PairName = RegisterPairName[0x3 & (Opcode >> 4)];
                if (Opcode & 0x08) /* 0b00RP_1010 */
                {
                    DisassembleWithOperand(Line, Opcode, "ldax", PairName, 2);
                }
                else /* 0b00RP_0010 */
                {
                    DisassembleWithOperand(Line, Opcode, "stax", PairName, 2);
                }
            } break;
            case 03:
            {
                const char *PairName = RegisterPairName[0x3 & (Opcode >> 4)];
                if (Opcode & 0x08) /* 0b00RP_1011 */
                {
                    DisassembleWithOperand(Line, Opcode, "dcx", PairName, 2);
                }
                else
                {
                    DisassembleWithOperand(Line, Opcode, "inx", PairName, 2);
                }
            } break;
            case 04:
            {
                const char *RegName = &RegisterName[0x3 & (Opcode >> 4)];
                DisassembleWithOperand(Line, Opcode, "inr", RegName, 1);
            } break;
            case 05:
            {
                const char *RegName = &RegisterName[0x3 & (Opcode >> 4)];
                DisassembleWithOperand(Line, Opcode, "dcr", RegName, 1);
            } break;
            case 06: /* 0b00DD_D110 */
            {
                char Dst = RegisterName[0x7 & (Opcode >> 3)];
                if (Next + 1 > End) 
                {
                    sprintf(Line, "%02x ??      mvi   %c, #??", Opcode, Dst);
                    Next = End;
                }
                else
                {
                    uint8_t Byte = *Next++;
                    sprintf(Line, "%02x %02x      mvi   %c, #%02x", 
                        Opcode, Byte, 
                        Dst, Byte
                    );
                }
            } break;
            default: PrintUnknownInstruction(Line, Opcode); break;
            }
        } break;
        case 2: /* 0b10mmmsss; m: mnemonic, s: source */
        {
            static const char Mnemonic[][4] = {
                "add", "adc", "sub", "sbb",
                "ana", "ora", "xra", "cmp"
            };
            DisassembleWithOperand(Line, Opcode, 
                Mnemonic[0x7 & (Opcode >> 3)], 
                &RegisterName[0x7 & Opcode], 1
            );
        } break;
        case 3:
        {
            switch (0x7 & Opcode)
            {
            case 00: /* 0b11ccc000: Rcc */
            {
                char Mnemonic[4] = { 0 };
                memcpy(Mnemonic + 1, &ConditionName[0x7 & (Opcode >> 3)], 2);
                Mnemonic[0] = 'r';
                DisassembleSingle(Line, Opcode, Mnemonic);
            } break;
            case 01:
            {
                unsigned RPIndex = 0x3 & (Opcode >> 4);
                if (0x3 == RPIndex)
                {
                    DisassembleSingle(Line, Opcode, "psw?");
                }
                else
                {
                    DisassembleWithOperand(Line, Opcode, "pop", RegisterPairName[RPIndex], 2);
                }
            } break;
            case 02: /* 0b11ccc010: Jcc */
            {
                char Mnemonic[4] = { 0 };
                memcpy(Mnemonic + 1, &ConditionName[0x7 & (Opcode >> 3)], 2);
                Mnemonic[0] = 'j';
                End = DisassembleAddr(Line, Opcode, Mnemonic, Next, End);
            } break;
            case 04: /* 0b11ccc100: Ccc */
            {
                char Mnemonic[4] = { 0 };
                memcpy(Mnemonic + 1, &ConditionName[0x7 & (Opcode >> 3)], 2);
                Mnemonic[0] = 'c';
                End = DisassembleAddr(Line, Opcode, Mnemonic, Next, End);
            } break;
            case 05: /* 0b11RP_0101: PUSH RP; PSW */
            {
                unsigned RPIndex = 0x3 & (Opcode >> 4);
                if (0x3 == RPIndex)
                {
                    DisassembleSingle(Line, Opcode, "psw");
                }
                else
                {
                    DisassembleWithOperand(Line, Opcode, "push", RegisterPairName[RPIndex], 2);
                }
            } break;
            /* case 6 is arith with immediate group */
            case 07: /* 0b11nnn111: RST n */
            {
                char n = 0x7 & (Opcode >> 3);
                n += '0';
                DisassembleWithOperand(Line, Opcode, "rst", &n, 1);
            } break;
            }
        } break;
        }
    } break;
    }

    if (Capitalized)
    {
        int i = 0;
        while (i < sizeof Line && Line[i] != '\0')
        {
            if (IN_RANGE('a', Line[i], 'z'))
            {
                Line[i] = Line[i] & ~(1 << 5);
            }
            i++;
        }
    }
    fprintf(OutStream, "%s", Line);
    return Next;
}

static void DisassembleBuffer(FILE *OutStream, Boolean32 Capitalized, const uint8_t *Buffer, size_t BufferSize)
{
    const uint8_t *Ptr = Buffer;
    unsigned i = 0;
    while (Ptr < Buffer + BufferSize)
    {
        fprintf(OutStream, "%04x: ", i);
        const uint8_t *NextPtr = DisassembleInstruction(OutStream, Capitalized, Ptr, Buffer + BufferSize);
        fputc('\n', OutStream);

        i += NextPtr - Ptr;
        Ptr = NextPtr;
    }
}


int main(int argc, char **argv)
{
    if (argc < 2)
    {
        DisPrintUsage(argv[0]);
        return 0;
    }
    const char *InputFileName = argv[1];
    FILE *f = fopen(InputFileName, "rb");
    if (NULL == f)
    {
        perror(InputFileName);
        return 1;
    }

    /* find file size */
    fseek(f, 0, SEEK_END);
    size_t FileSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    static uint8_t Buffer[0x10000] = { 0 };
    if (FileSize > sizeof Buffer)
    {
        printf("File size must be less than the 8080's addressable range.\n");
        return 1;
    }
    if (fread(Buffer, 1, FileSize, f) != FileSize)
    {
        printf("Error in reading file.\n");
        return 1;
    }

    Boolean32 CapitalizeInstructions = true;
    DisassembleBuffer(stdout, CapitalizeInstructions, Buffer, FileSize);

    fclose(f);
    return 0;
}

