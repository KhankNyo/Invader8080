#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>

#ifdef PLATFORM_WIN32
#  include "Win32.h"
#else
#  error "TODO: other platforms"
#endif

typedef struct PlatformBackBuffer 
{
    uint32_t Width, Height;
    uint32_t *Data;
} PlatformBackBuffer;

typedef unsigned char Bool8;
#define false 0
#define true 1

double Platform_GetTimeMillisec(void);
void Platform_SetBackBufferDimension(uint32_t Width, uint32_t Height);
PlatformBackBuffer Platform_GetBackBuffer(void);
void Platform_SwapBuffer(const PlatformBackBuffer *BackBuffer);

Bool8 Platform_SoundDeviceIsReady(void);
void Platform_WriteToSoundDevice(const void *SoundBuffer, size_t SoundBufferSize);

void Platform_FatalError(const char *ErrorMessage);
void Platform_PrintError(const char *ErrorMessage);
void Platform_Sleep(unsigned Millisec);


void Invader_Loop(void);
void Invader_OnKeyDown(PlatformKey Key);
void Invader_OnKeyUp(PlatformKey Key);


#endif /* PLATFORM_H */

