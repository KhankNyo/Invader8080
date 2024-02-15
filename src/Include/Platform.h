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

typedef struct PlatformSoundBuffer 
{
    int16_t *Buffer;
    size_t BufferSizeBytes;
    uint32_t SamplePerSec;
    uint32_t SampleSize;
} PlatformSoundBuffer;

double Platform_GetTimeMillisec(void);
void *Platform_GetBackBuffer(void);
void Platform_SwapBuffer(void);

void Platform_PlaySound(void);

void Platform_Exit(int ExitCode);
void Platform_PrintError(const char *ErrorMessage);


#endif /* PLATFORM_H */

