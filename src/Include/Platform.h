#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>

#ifdef PLATFORM_WIN32
#  include "Win32.h"
#else
#  error "TODO: other platforms"
#endif

typedef unsigned char Bool8;
#define false 0
#define true 1

double Platform_GetTimeMillisec(void);
void *Platform_GetBackBuffer(void);
void Platform_SwapBuffer(void);

void Platform_WriteToSoundDevice(const void *SoundBuffer, size_t SoundBufferSize);

void Platform_PrintError(const char *ErrorMessage);
void Platform_Exit(int ExitCode);


void Invader_Loop(void);
void Invader_OnKeyDown(PlatformKey Key);
void Invader_OnKeyUp(PlatformKey Key);
void Invader_OnSoundEnd(double CurrentTimeMillisec);


#endif /* PLATFORM_H */

