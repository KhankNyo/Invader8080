
#include <stdio.h>
#include <stdlib.h>
#include "Common.h"

typedef struct FileInfo 
{
    char *Buffer;
    size_t Size;
} FileInfo;

size_t SizeOfFile(FILE *f)
{
    fseek(f, 0, SEEK_END);
    size_t FileSize = ftell(f);
    fseek(f, 0, SEEK_SET);
    return FileSize;
}

FileInfo FileRead(const char *FileName, bool IsTextFile)
{
    IsTextFile = 0 != IsTextFile; /* ensure 0 or 1 */
    FileInfo Info = { 0 };
    FILE *f = fopen(FileName, "rb");
    if (NULL == f)
        return Info;

    /* get file size */
    size_t FileSize = SizeOfFile(f);

    /* allocate buffer */
    Info.Buffer = malloc(FileSize + (0 != IsTextFile));
    if (NULL == Info.Buffer)
        goto CloseFile;

    if (fread(Info.Buffer, 1, FileSize, f) != FileSize)
    {
        free(Info.Buffer);
        Info.Buffer = NULL;
        goto CloseFile;
    }

    Info.Size = FileSize;
    /* ensure null termination */
    if (IsTextFile)
    {
        Info.Buffer[FileSize] = '\0';
        Info.Size += 1;
    }

CloseFile:
    fclose(f);
    return Info;
}

void FileCleanup(FileInfo *File)
{
    free(File->Buffer);
    *File = (FileInfo) { 0 };
}

bool ReadFileIntoBuffer(void *Buffer, size_t BufferSize, const char *FileName)
{
    FILE *f = fopen(FileName, "rb");
    if (NULL == Buffer)
        return false;

    size_t FileSize = SizeOfFile(f);
    if (BufferSize < FileSize)
    {
        fprintf(stderr, "Buffer too small for %s\n", FileName);
        exit(1);
    }

    if (fread(Buffer, 1, FileSize, f) != FileSize)
    {
        fclose(f);
        return false;
    }

    fclose(f);
    return true;
}

