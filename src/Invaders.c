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
    W_SHIFTAMNT     = 2,  
    W_SOUND1        = 3,         
    W_SHIFT_DATA    = 4,       
    W_SOUND2        = 5,         
    W_WATCHDOG      = 6,       
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




#define WHITE 0x00FFFFFFul
#define BLACK 0x00000000ul
#define GREEN_MASK 0x0000FF00ul
#define YELLOW_MASK 0x00FFFF00ul
#define CLOCK_RATE 2000000ul
#define WAVE_FILE_DATA_OFFSET 44

static PortHardware sHardware = { 0 };
static Intel8080 sI8080 = { 0 };
static uint8_t sRam[0x2400 - 0x2000];
static uint8_t sVideoMemory[0x4000 - 0x2400];

static uint8_t sCurrentSoundBuffer;
static int16_t sSoundBuffer[16][200 * 1024];
static uint32_t sSoundBufferSizeBytes;
static const uint8_t *sLoopingSound;
static uint32_t sLoopingSoundSizeBytes;




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

static void MemoryCopy(void *Dst, const void *Src, size_t SizeBytes)
{
    uint8_t *DstPtr = Dst;
    const uint8_t *SrcPtr = Src;
    while (SizeBytes--)
    {
        *DstPtr++ = *SrcPtr++;
    }
}

static void PushSound(const uint8_t *SoundDataBytes, size_t SoundDataSizeBytes)
{
#define MIN(a, b) a > b? b : a
    static int16_t *SoundBufferPtr;
    const int16_t *SoundData = (const int16_t*)(SoundDataBytes + WAVE_FILE_DATA_OFFSET);
    SoundBufferPtr = sSoundBuffer[sCurrentSoundBuffer];
    SoundDataSizeBytes -= WAVE_FILE_DATA_OFFSET;
    if (sSoundBufferSizeBytes == 0)
    {
        MemoryCopy(sSoundBuffer[sCurrentSoundBuffer], SoundData, SoundDataSizeBytes);
        sSoundBufferSizeBytes = SoundDataSizeBytes;
    }
    else
    {
        /* mix sounds together */
        unsigned SoundSampleSize = sizeof *SoundData;
        size_t MinSize = MIN(sSoundBufferSizeBytes, SoundDataSizeBytes);
        for (unsigned i = 0; i < MinSize / SoundSampleSize; i++)
        {
            *SoundBufferPtr++ += *SoundData++;
        }

        /* copy the residue sound */
        if (SoundDataSizeBytes > MinSize)
        {
            MemoryCopy(SoundBufferPtr, SoundData, SoundDataSizeBytes - MinSize);
            sSoundBufferSizeBytes = SoundDataSizeBytes;
        }
    }
#undef MIN
}

/* submits sound buffer to the sound device, also adds the looping sound */
static void SubmitSound(void)
{
    unsigned LoopingSizeBytes = (sLoopingSoundSizeBytes - WAVE_FILE_DATA_OFFSET);
    const int16_t *LoopingSound = (const int16_t *)(sLoopingSound + WAVE_FILE_DATA_OFFSET);
    static unsigned SampleIndex = 0;
    if (NULL == sLoopingSound)
        SampleIndex = 0;

    if (sSoundBufferSizeBytes == 0)
    {
        if (sLoopingSound)
        {
            Platform_WriteToSoundDevice(LoopingSound, LoopingSizeBytes);
        }
    }
    else
    {
        if (sLoopingSound)
        {
            int16_t *SoundBuffer = sSoundBuffer[sCurrentSoundBuffer];
            for (unsigned i = 0; i < sSoundBufferSizeBytes / sizeof *SoundBuffer; i++)
            {
                *SoundBuffer++ += LoopingSound[SampleIndex++ % (LoopingSizeBytes/sizeof *LoopingSound)];
            }
        }
        Platform_WriteToSoundDevice(sSoundBuffer[sCurrentSoundBuffer], sSoundBufferSizeBytes);
        sCurrentSoundBuffer++;
        sCurrentSoundBuffer %= STATIC_ARRAY_SIZE(sSoundBuffer);
    }
    sSoundBufferSizeBytes = 0;
}

static void PortWriteByte(Intel8080 *i8080, uint16_t Port, uint8_t Byte)
{
#define ON_RISING_EDGE(Curr, Prev, Bit) ((Curr & (1 << Bit)) && 0 == (Prev & (1 << Bit)))
#define ON_FALLING_EDGE(Curr, Prev, Bit) ((Curr & (1 << Bit)) == 0 && (Prev & (1 << Bit)))

    (void)i8080;
    switch (Port)
    {
    case W_SOUND1: 
    {
        static uint8_t Last = 0;
        if (ON_RISING_EDGE(Byte, Last, 0))
        {
            /* UFO sound loops */
            sLoopingSound = gUFOSound;
            sLoopingSoundSizeBytes = gUFOSoundSize;
        }
        if (ON_RISING_EDGE(Byte, Last, 1))
        {
            PushSound(gShotSound, gShotSoundSize);
        }
        if (ON_RISING_EDGE(Byte, Last, 2))
        {
            PushSound(gPlayerDieSound, gPlayerDieSoundSize);
        }
        if (ON_RISING_EDGE(Byte, Last, 3))
        {
            PushSound(gInvaderDieSound, gInvaderDieSoundSize);
        }

        if (ON_FALLING_EDGE(Byte, Last, 0))
        {
            sLoopingSound = NULL;
            sLoopingSoundSizeBytes = 0;
        }
        Last = Byte;
    } break;
    case W_SOUND2: 
    {
        static uint8_t Last = 0;
        if (ON_RISING_EDGE(Byte, Last, 0))
        {
            PushSound(gFleet1Sound, gFleet1SoundSize);
        }
        if (ON_RISING_EDGE(Byte, Last, 1))
        {
            PushSound(gFleet2Sound, gFleet2SoundSize);
        }
        if (ON_RISING_EDGE(Byte, Last, 2))
        {
            PushSound(gFleet3Sound, gFleet3SoundSize);
        }
        if (ON_RISING_EDGE(Byte, Last, 3))
        {
            PushSound(gFleet4Sound, gFleet4SoundSize);
        }
        if (ON_RISING_EDGE(Byte, Last, 4))
        {
            PushSound(gUFOHitSound, gUFOHitSoundSize);
        }
        Last = Byte;
    } break;
    case W_SHIFTAMNT:   sHardware.SR.ShiftAmount = Byte & 0x7; break;
    case W_SHIFT_DATA:  sHardware.SR.Data = ((uint16_t)Byte << 8) | (sHardware.SR.Data >> 8); break;
    }
#undef ON_RISING_EDGE
#undef ON_FALLING_EDGE
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
    case KEY_2:     sHardware.Player1 |= 1 << 1; break; /* player 2 start */
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
        Platform_FatalError("Corrupted rom (size != 8192).");
    }

    sI8080 = I8080Init(0, NULL, 
        MemReadByte, MemWriteByte, 
        PortReadByte, PortWriteByte
    );
    sHardware.Player2 = 0x03; /* 6 ships */

    Platform_SetBackBufferDimension(224, 256);
}

void Invader_Loop(void)
{
    static unsigned Cycles = 0;
    uint32_t FPSTarget = 60;
    uint32_t CyclesPerFrames = CLOCK_RATE / FPSTarget;

    Cycles++;
    I8080AdvanceClock(&sI8080);

    if ((Cycles % (CyclesPerFrames / 5)) == 0 
    && Platform_SoundDeviceIsReady())
    {
        SubmitSound();
    }

    /* mid-frame interrupt */
    if (Cycles == CyclesPerFrames / 2) 
    {
        I8080Interrupt(&sI8080, 1);
    }

    /* 1 frame finished */
    if (Cycles == CyclesPerFrames)
    {
        PlatformBackBuffer BackBuffer = Platform_GetBackBuffer();
        uint32_t *Buffer = BackBuffer.Data;
        for (int UnrotatedX = BackBuffer.Height - 1; UnrotatedX >= 0; UnrotatedX--)
        {
            for (int UnrotatedY = 0; UnrotatedY < (int)BackBuffer.Width; UnrotatedY++)
            {
                /* ARGB */
                int VideoIndex = UnrotatedX + UnrotatedY * BackBuffer.Height;
                uint32_t Pixel = 1 & (sVideoMemory[VideoIndex/8] >> (VideoIndex % 8))?
                    WHITE: BLACK;

                if (UnrotatedX < 80)
                    Pixel &= GREEN_MASK;
                if (UnrotatedX > 180)
                    Pixel &= YELLOW_MASK;

                *Buffer++ = Pixel;
            }
        }
        Platform_SwapBuffer(&BackBuffer);

        /* screen interrupt */
        I8080Interrupt(&sI8080, 2);

        static double sStartTime = 0;
        double ElapsedTime = Platform_GetTimeMillisec() - sStartTime;
        if (ElapsedTime < 1000.0 / FPSTarget)
        {
            Platform_Sleep(1000.0 / FPSTarget - ElapsedTime);
        }
        sStartTime = Platform_GetTimeMillisec();

        Cycles = 0;
    }
}

