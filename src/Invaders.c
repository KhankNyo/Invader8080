

#ifdef STANDALONE
#  error "Invaders.c cannot be compiled as a standalone file, it should be included in the platform files."
#endif /* STANDALONE */

#include "Resources.h"
#include "Platform.h"
#include "8080.c"




typedef enum InvaderReadPort
{
    R_INP0 = 0,       
    R_INP1 = 1,           
    R_INP2 = 2,           
    R_SHIFT_IN = 3,       
} InvaderReadPort;

typedef enum InvaderWritePort
{
    W_SHIFTAMNT = 2,  
    W_SOUND1 = 3,         
    W_SHIFT_DATA = 4,       
    W_SOUND2 = 5,         
    W_WATCHDOG = 6,       
} InvaderWritePort;

typedef struct ShiftRegister 
{
    unsigned ShiftAmount;
    uint16_t Data;
} ShiftRegister;

typedef struct PortHardware 
{
    ShiftRegister SR;
    uint8_t Player1, 
            Player2;
} PortHardware;

typedef struct SoundNode 
{
    const uint8_t *Buffer;
    size_t BufferSize;
} SoundNode;
static PortHardware sHardware = { 0 };
static Intel8080 sI8080 = { 0 };
static uint8_t sRam[0x2400 - 0x2000];
static uint8_t sVideoMemory[0x4000 - 0x2400];

static SoundNode sSoundNodeList[256];
static size_t sSoundNodeCount;




static uint8_t MemReadByte(Intel8080 *i8080, uint16_t Address)
{
    (void)i8080;
    Address %= 0x4000;
    if (Address < 0x1FFF)
    {
        return gSpaceInvadersRom[Address];
    }
    if (IN_RANGE(0x2000, Address, 0x23FF))
    {
        return sRam[Address - 0x2000];
    }
    if (IN_RANGE(0x2400, Address, 0x3FFF))
    {
        return sVideoMemory[Address - 0x2400];
    }
    return 0;
}

static void MemWriteByte(Intel8080 *i8080, uint16_t Address, uint8_t Byte)
{
    (void)i8080;
    Address %= 0x4000;
    if (Address < 0x1FFF)
    {
        /* writes to rom are ignored */
    }
    else if (IN_RANGE(0x2000, Address, 0x23FF))
    {
        sRam[Address - 0x2000] = Byte;
    }
    else if (IN_RANGE(0x2400, Address, 0x3FFF))
    {
        sVideoMemory[Address - 0x2400] = Byte;
    }
}


static uint8_t PortReadByte(Intel8080 *i8080, uint16_t Port)
{
    (void)i8080;
    uint8_t Byte = 0;
    switch ((InvaderReadPort)Port)
    {
    case R_INP0:        Byte = 1; break;
    case R_INP1:        Byte = sHardware.Player1; break;
    case R_INP2:        Byte = sHardware.Player2; break;
    case R_SHIFT_IN:    Byte = sHardware.SR.Data >> (8 - sHardware.SR.ShiftAmount); break;
    }
    return Byte;
}

static void PushWAVSound(const void *WAVSound, size_t SizeInBytes)
{
    const uint8_t *WAVSoundBytes = WAVSound;
    /* where data section of WAV sound file starts */
    int WAVDataSectionOffset = 44;

    DEBUG_ASSERT(sSoundNodeCount < STATIC_ARRAY_SIZE(sSoundNodeList));
    DEBUG_ASSERT(SizeInBytes < WAVDataSectionOffset);

    SoundNode *CurrentSound = &sSoundNodeList[sSoundNodeCount++];
    CurrentSound->Buffer = WAVSoundBytes + WAVDataSectionOffset;
    CurrentSound->BufferSize = SizeInBytes - WAVDataSectionOffset;
}


static void PortWriteByte(Intel8080 *i8080, uint16_t Port, uint8_t Byte)
{
#define ON_EDGE(Curr, Prev, Bit) ((Curr & (1 << Bit)) && 0 == (Prev & (1 << Bit)))
    (void)i8080;
    switch (Port)
    {
    case W_SOUND1: 
    {
        static uint8_t Last = 0;
        if (ON_EDGE(Byte, Last, 0))
        {
            PushWAVSound(gUFOSound, gShotSoundSize);
        }
        if (ON_EDGE(Byte, Last, 1))
        {
            PushWAVSound(gShotSound, gShotSoundSize);
        }
        if (ON_EDGE(Byte, Last, 2))
        {
        }
        if (ON_EDGE(Byte, Last, 3))
        {
        }
        Last = Byte;
    } break;
    case W_SOUND2: 
    {
        static uint8_t Last = 0;
        if (ON_EDGE(Byte, Last, 4))
        {
        }
        if (ON_EDGE(Byte, Last, 5))
        {
        }
        if (ON_EDGE(Byte, Last, 6))
        {
        }
        if (ON_EDGE(Byte, Last, 7))
        {
        }
        if (ON_EDGE(Byte, Last, 8))
        {
        }
        Last = Byte;
    } break;
    case W_SHIFTAMNT:   sHardware.SR.ShiftAmount = Byte & 0x7; break;
    case W_SHIFT_DATA:  sHardware.SR.Data = ((uint16_t)Byte << 8) | (sHardware.SR.Data >> 8); break;
    }
#undef ON_EDGE
}


void Invader_OnKeyUp(PlatformKey Key)
{
    switch ((int)Key)
    {
    case KEY_C:     sHardware.Player1 &= ~1; break; /* coin */
    case KEY_T:     sHardware.Player2 &= ~(1 << 2); break; /* tilt */

    case KEY_2:     sHardware.Player1 &= ~(1 << 1); break; /* player 2 start */
    case KEY_1:     sHardware.Player1 &= ~(1 << 2); break; /* player start */

    case KEY_A:     sHardware.Player1 &= ~(1 << 5); break;
    case KEY_D:     sHardware.Player1 &= ~(1 << 6); break;
    case KEY_LEFT:  sHardware.Player2 &= ~(1 << 5); break;
    case KEY_RIGHT: sHardware.Player2 &= ~(1 << 6); break;

    case KEY_SPACE: sHardware.Player1 &= ~(1 << 4); break; /* shoot */
    case KEY_ENTER: sHardware.Player2 &= ~(1 << 4); break; /* shoot */
    }

}

void Invader_OnKeyDown(PlatformKey Key)
{
    switch ((int)Key)
    {
    case KEY_C:     sHardware.Player1 |= 1 << 0; break; /* coin */
    case KEY_T:     sHardware.Player2 |= 1 << 2; break; /* tilt */
    case KEY_2:     sHardware.Player2 |= 1 << 1; break; /* player 2 start */
    case KEY_1:     sHardware.Player1 |= 1 << 2; break; /* player 1 start */

    case KEY_A:     sHardware.Player1 |= 1 << 5; break;
    case KEY_D:     sHardware.Player1 |= 1 << 6; break;
    case KEY_LEFT:  sHardware.Player2 |= 1 << 5; break;
    case KEY_RIGHT: sHardware.Player2 |= 1 << 6; break;
    case KEY_R:     I8080Interrupt(&sI8080, 0); break; /* reset */

    case KEY_SPACE: sHardware.Player1 |= 1 << 4; break; /* shoot */
    case KEY_ENTER: sHardware.Player2 |= 1 << 4; break; /* shoot */
    }
}



void Invader_Setup(void)
{
    if (gSpaceInvadersRomSize != 0x2000)
    {
        Platform_PrintError("Corrupted rom (size != 8192).");
        Platform_Exit(1);
    }

    sI8080 = I8080Init(0, NULL, 
        MemReadByte, MemWriteByte, 
        PortReadByte, PortWriteByte
    );

    sHardware.Player1 = 1 << 2; /* player 1 start */
    sHardware.Player2 = 0x03; /* 6 ships */


}

static void MixAndPlaySound(PlatformSoundBuffer *Sound)
{
    Platform_ClearSoundBuffer(Sound);
    size_t UnfinishedSoundCount = 0;
    for (unsigned i = 0; i < sSoundNodeCount; i++)
    {
        const uint8_t *Buffer = sSoundNodeList[i].Buffer;
        size_t BufferSize = sSoundNodeList[i].BufferSize;
        if (BufferSize > Sound->BufferSizeBytes)
        {
            /* truncate the buffer */
            sSoundNodeList[i].Buffer += Sound->BufferSizeBytes;
            sSoundNodeList[i].BufferSize -= Sound->BufferSizeBytes;
            BufferSize = Sound->BufferSizeBytes;

            /* 'push' the unfinished sound back into the buffer */
            sSoundNodeList[UnfinishedSoundCount++] = sSoundNodeList[i];
        }
        Platform_MixSoundBuffer(Sound, Buffer, BufferSize);
    }

    sSoundNodeCount = UnfinishedSoundCount;
}


void Invader_Loop(void)
{
    static unsigned Cycles = 0;
    Cycles++;
    I8080AdvanceClock(&sI8080);

    /* mid-frame interrupt */
    if (Cycles == 16666) 
    {
        I8080Interrupt(&sI8080, 1);
    }

    /* TODO: remove all these magic-ass numbers, they're making me sick */
    /* 1 frame finished */
    if (Cycles == 33333)
    {
        uint32_t *Buffer = Platform_GetBackBuffer();
        for (int UnrotatedX = 255; UnrotatedX > 0; UnrotatedX--)
        {
            for (int UnrotatedY = 0; UnrotatedY < 224; UnrotatedY++)
            {
                /* ARGB */
                int VideoIndex = UnrotatedX + UnrotatedY*256;
                uint32_t Pixel = 1 & (sVideoMemory[VideoIndex/8] >> (VideoIndex % 8))?
                    0x00FFFFFF: 0x00000000;
                if (Pixel && UnrotatedX < 80)
                    Pixel = 0x0000FF00;
                *Buffer++ = Pixel;
            }
        }
        Platform_SwapBuffer();
        I8080Interrupt(&sI8080, 2);

        Cycles = 0;
        static double sStartTime = 0;
        double ElapsedTime = Platform_GetTimeMillisec() - sStartTime;
        if (ElapsedTime < 1000.0 / 60.0)
        {
            Sleep(1000.0 / 60.0 - ElapsedTime);
        }
        sStartTime = Platform_GetTimeMillisec();

        PlatformSoundBuffer *Sound = Platform_RetrieveSoundBuffer(1000 / 60);
        if (NULL != Sound)
        {
            MixAndPlaySound(Sound);
            Platform_CommitSoundBuffer(Sound);
        }
    }
}

