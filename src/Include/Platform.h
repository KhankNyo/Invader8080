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

typedef struct PlatformSound PlatformSound;

double Platform_GetTimeMillisec(void);
void *Platform_GetBackBuffer(void);
void Platform_SwapBuffer(void);
void Platform_FillSoundBuffer(
    PlatformSound *Buffer, uint32_t BytesToLock, uint32_t BytesToWrite
);

PlatformSound *Platform_CreateSound(const void *SoundBuffer, size_t BufferSize, Bool8 Looped);
Bool8 Platform_PlaySound(PlatformSound *Sound);
void Platform_DestroySound(PlatformSound *Sound);

void Platform_Exit(int ExitCode);
void Platform_PrintError(const char *ErrorMessage);


#endif /* PLATFORM_H */

