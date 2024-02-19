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

typedef struct SoundSample 
{
    int16_t *Data;
    uint32_t SampleCount;
    uint32_t Index;
    double TimeToStartPlaying;
} SoundSample;




#define WHITE 0x00FFFFFFul
#define BLACK 0x00000000ul
#define GREEN_MASK 0x0000FF00ul
#define YELLOW_MASK 0x00FFFF00ul

#define CLOCK_RATE 2000000ul
#define FRAME_TIME_TARGET 1000.0 / 60.0
#define CYCLES_PER_FRAME CLOCK_RATE / 60
#define WAVE_FILE_DATA_OFFSET 44




static PortHardware sHardware = { 0 };
static Intel8080 sI8080 = { 0 };
static uint32_t sCycles = 0;
static uint8_t sRam[0x2400 - 0x2000];
static uint8_t sVideoMemory[0x4000 - 0x2400];

static Bool8 sHasSound;
/* NOTE: SampleBufferCount will never be greater than 256 (highest record was 7) */
static PLATFORM_ATOMIC uint8_t sSampleBufferCount;
static SoundSample sSampleBuffer[256];
static SoundSample sLoopingSample;
static PlatformCriticalSection *sPushSoundCriticalSection;



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

static void PushSound(const uint8_t *SoundDataBytes, size_t SoundDataSizeBytes)
{
    SoundSample Sample = {
        .Data = (int16_t *)(SoundDataBytes + 44),
        .SampleCount = (SoundDataSizeBytes - 44) / 2,
        .Index = 0,
    };

    Platform_EnterCriticalSection(sPushSoundCriticalSection);
        sSampleBuffer[sSampleBufferCount++] = Sample;
    Platform_LeaveCriticalSection(sPushSoundCriticalSection);
}

static void PushLoopingSound(const uint8_t *SoundDataBytes, size_t SoundDataSizeBytes)
{
    SoundSample Sample = {
        .Data = (int16_t *)(SoundDataBytes + 44),
        .SampleCount = (SoundDataSizeBytes - 44) / 2,
        .Index = 0,
    };

    Platform_EnterCriticalSection(sPushSoundCriticalSection);
        sLoopingSample = Sample;
    Platform_LeaveCriticalSection(sPushSoundCriticalSection);
}

static void PopLoopingSound(const uint8_t *SoundDataBytes, size_t SoundDataSizeBytes)
{
    (void)SoundDataBytes, (void)SoundDataSizeBytes;
    Platform_EnterCriticalSection(sPushSoundCriticalSection);
        sLoopingSample.Data = NULL;
    Platform_LeaveCriticalSection(sPushSoundCriticalSection);
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
            PushLoopingSound(gUFOSound, gUFOSoundSize);
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
            PopLoopingSound(gUFOSound, gUFOSoundSize);
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

int16_t Invader_OnSoundThreadRequestingSample(double CurrentTime, double TimeStep)
{
    int32_t IntermediateSoundSample = 0;
    Platform_EnterCriticalSection(sPushSoundCriticalSection);
    {
        for (int i = 0; i < (int)sSampleBufferCount; i++)
        {
            /* pop the sound sample that has been fully played */
            if (sSampleBuffer[i].Index >= sSampleBuffer[i].SampleCount)
            {
                sSampleBuffer[i] = sSampleBuffer[--sSampleBufferCount];
            }
            else
            {
                IntermediateSoundSample += sSampleBuffer[i].Data[
                    sSampleBuffer[i].Index++
                ];
            }
        }
        if (NULL != sLoopingSample.Data)
        {
            if (sLoopingSample.Index >= sLoopingSample.SampleCount)
                sLoopingSample.Index = 0;
            IntermediateSoundSample += sLoopingSample.Data[
                sLoopingSample.Index++
            ];
        }
    }
    Platform_LeaveCriticalSection(sPushSoundCriticalSection);

    if (IntermediateSoundSample > INT16_MAX)
        return INT16_MAX;
    else if (IntermediateSoundSample < INT16_MIN)
        return INT16_MIN;
    else return IntermediateSoundSample;
}

void Invader_Setup(PlatformAudioFormat *AudioFormat)
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

    sHasSound = true;
    AudioFormat->ShouldHaveSound = true;
    AudioFormat->SampleRate = 44100;    /* the resources' sample rate */
    AudioFormat->ChannelCount = 2;      /* stereo */
    AudioFormat->QueueSize = 48;         /* magic */
    AudioFormat->BufferSizeBytes = 128 * AudioFormat->ChannelCount * sizeof(int16_t); /* magic */

    sPushSoundCriticalSection = Platform_CreateCriticalSection();
}

void Invader_OnAudioInitializationFailed(const char *ErrorMessage)
{
    Platform_PrintError(ErrorMessage);
    sHasSound = false;
}


static void WaitTime(double ElapsedTime, double ExpectedTime)
{
    if (ElapsedTime < ExpectedTime)
    {
        double ResidueTime = ExpectedTime - ElapsedTime;
        Platform_Sleep(ResidueTime);
    }
}

void Invader_Loop(void)
{
    static double StartTime = 0;
    sCycles++;
    I8080AdvanceClock(&sI8080);


    /* mid-frame interrupt */
    if (sCycles == CYCLES_PER_FRAME / 2) 
    {
        I8080Interrupt(&sI8080, 1);
        double ElapsedTime = Platform_GetTimeMillisec() - StartTime;
        WaitTime(ElapsedTime, FRAME_TIME_TARGET / 2);
    }

    /* 1 frame finished */
    if (sCycles == CYCLES_PER_FRAME)
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
        sCycles = 0;

        double ElapsedTime = Platform_GetTimeMillisec() - StartTime;
        WaitTime(ElapsedTime, FRAME_TIME_TARGET);
        StartTime = Platform_GetTimeMillisec();
    }


}

void Invader_AtExit(void)
{
    Platform_DestroyCriticalSection(sPushSoundCriticalSection);
}

