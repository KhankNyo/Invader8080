#ifndef PLATFORM_H
#define PLATFORM_H

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

void Platform_Exit(int ExitCode);
void Platform_PrintError(const char *ErrorMessage);
void Platform_PrintPlatformError(void);

PlatformFile Platform_OpenFile(const char *FileName, PlatformFilePermission Perm);
Bool8 Platform_InvalidFile(PlatformFile File);
size_t Platform_ReadFile(PlatformFile File, void *Buffer, size_t BufferSize);
void Platform_CloseFile(PlatformFile File);



#endif /* PLATFORM_H */

