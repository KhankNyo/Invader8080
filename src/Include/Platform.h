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
    size_t WrittenSizeBytes;
    uint32_t SamplePerSec;
    uint32_t SampleSize;
} PlatformSoundBuffer;

double Platform_GetTimeMillisec(void);
void *Platform_GetBackBuffer(void);
void Platform_SwapBuffer(void);

PlatformSoundBuffer *Platform_RetrieveSoundBuffer(unsigned SampleDurationInMillisec);
void Platform_ClearSoundBuffer(PlatformSoundBuffer *Sound);
void Platform_MixSoundBuffer(PlatformSoundBuffer *Sound, const void *Data, size_t DataSize);
void Platform_CommitSoundBuffer(PlatformSoundBuffer *Sound);

void Platform_Exit(int ExitCode);
void Platform_PrintError(const char *ErrorMessage);


#endif /* PLATFORM_H */

