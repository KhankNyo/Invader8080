#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>

#ifdef PLATFORM_WIN32
#  include "Win32.h"
#else
#  error "TODO: other platforms"
#endif


typedef enum PlatformKey PlatformKey;
typedef struct PlatformCriticalSection PlatformCriticalSection;

typedef unsigned char Bool8;
#define false 0
#define true 1

typedef struct PlatformBackBuffer 
{
    uint32_t Width, Height;
    uint32_t *Data;
} PlatformBackBuffer;

typedef struct PlatformAudioFormat 
{
    Bool8 ShouldHaveSound;
    uint32_t SampleRate;
    uint32_t ChannelCount;
    uint32_t QueueSize;
    uint32_t BufferSizeBytes;
} PlatformAudioFormat;


double Platform_GetTimeMillisec(void);
void Platform_SetBackBufferDimension(uint32_t Width, uint32_t Height);
PlatformBackBuffer Platform_GetBackBuffer(void);
void Platform_SwapBuffer(const PlatformBackBuffer *BackBuffer);

void Platform_FatalError(const char *ErrorMessage);
void Platform_PrintError(const char *ErrorMessage);
void Platform_Sleep(unsigned Millisec);

PlatformCriticalSection *Platform_CreateCriticalSection(void);
void Platform_EnterCriticalSection(PlatformCriticalSection *Crit);
void Platform_LeaveCriticalSection(PlatformCriticalSection *Crit);
void Platform_DestroyCriticalSection(PlatformCriticalSection *Crit);


void Invader_Setup(PlatformAudioFormat *AudioFormat);
void Invader_OnAudioInitializationFailed(const char *ErrorMessage);
void Invader_Loop(void);
void Invader_AtExit(void);
void Invader_OnKeyDown(PlatformKey Key);
void Invader_OnKeyUp(PlatformKey Key);
/* NOTE: the platform may request sound from a different thread */
int16_t Invader_OnSoundThreadRequestingSample(double CurrentTime, double TimeStep);


#ifdef __STDC_NO_ATOMICS__
#  define PLATFORM_ATOMIC
#  define PLATFORM_ATOMIC_RMW(pCrit, Expr) do {\
    PlatformCriticalSection *CriticalSection_ = pCrit;\
    Platform_EnterCriticalSection(CriticalSection_);\
    Expr;\
    Platform_LeaveCriticalSection(CriticalSection_);\
} while (0)
#  define PLATFORM_ATOMIC_START(pCrit) Platform_EnterCriticalSection(pCrit)
#  define PLATFORM_ATOMIC_END(pCrit) Platform_LeaveCriticalSection(pCrit)
#else
#  define PLATFORM_ATOMIC _Atomic
#  define PLATFORM_ATOMIC_RMW(pCrit, Expr) (Expr)
#  define PLATFORM_ATOMIC_START(pCrit)
#  define PLATFORM_ATOMIC_END(pCrit)
#endif /* __STDC_NO_ATOMICS__ */




#endif /* PLATFORM_H */

