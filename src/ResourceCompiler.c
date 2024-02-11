
/*
 * takes in a binary file and convert it into a C file containing an array of bytes 
 * */
#include <stdio.h>
#include "File.c"

static void PrintUsage(const char *ProgramName)
{
    printf("Usage: %s <output source file> <output header file>\n", 
        ProgramName
    );
}

static bool CompileResource(
    const char *InputFileName, const char *ResourceIdentifier, 
    FILE *OutputSource, FILE *OutputHeader)
{
    FileInfo InputFile = FileRead(InputFileName, false);
    if (NULL == InputFile.Buffer)
    {
        perror(InputFileName);
        return false;
    }


    /* source */
    fprintf(OutputSource, 
        "\n"
        "\n"
        "const unsigned char %s[] = {",
        ResourceIdentifier
    );
    for (size_t i = 0; i < InputFile.Size; i++)
    {
        if (i % 8 == 0)
        {
            fprintf(OutputSource, "\n    ");
        }
        fprintf(OutputSource, "0x%02x, ", ((unsigned)InputFile.Buffer[i]) & 0xFF);
    }
    fprintf(OutputSource, 
            "\n"
            "};\n"
    );
    fprintf(OutputSource, "const unsigned long %sSize = sizeof %s;\n",
        ResourceIdentifier, ResourceIdentifier
    );


    /* header */
    fprintf(OutputHeader, "extern const unsigned char %s[];\n", ResourceIdentifier);
    fprintf(OutputHeader, "extern const unsigned long %sSize;\n", ResourceIdentifier);

    FileCleanup(&InputFile);
    return true;
}

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        PrintUsage(argv[0]);
        return 1;
    }
    const char *Resources[] = {
        "resources/Invaders.bin",
        "resources/0.wav",
        "resources/1.wav",
        "resources/2.wav",
        "resources/3.wav",
        "resources/4.wav",
        "resources/5.wav",
        "resources/6.wav",
        "resources/7.wav",
        "resources/8.wav",
    };
    const char *ResourceIdentifiers[] = {
        "gSpaceInvadersRom",
        "gUFOSound",
        "gShotSound",
        "gPlayerDieSound",
        "gInvaderDieSound",
        "gFleet1Sound",
        "gFleet2Sound",
        "gFleet3Sound",
        "gFleet4Sound",
        "gUFOHitSound",
    };
    if (STATIC_ARRAY_SIZE(Resources) != STATIC_ARRAY_SIZE(ResourceIdentifiers))
        UNREACHABLE("file count != resource name count");
    int RawResourcesCount = STATIC_ARRAY_SIZE(Resources);

    const char *SourceName = argv[1];
    const char *HeaderName = argv[2];
    
    FILE *SourceFile = fopen(SourceName, "wb");
    if (NULL == SourceFile)
    {
        perror(SourceName);
        return 1;
    }
    FILE *HeaderFile = fopen(HeaderName, "wb");
    if (NULL == HeaderFile)
    {
        perror(HeaderName);
        fclose(SourceFile);
        return 1;
    }

    const char *HeaderGuard = "RESOURCES_H";
    fprintf(HeaderFile, 
        "#ifndef %s\n"
        "#define %s\n", 
        HeaderGuard, 
        HeaderGuard
    );
    for (int i = 0; i < RawResourcesCount; i++)
    {
        if (!CompileResource(Resources[i], ResourceIdentifiers[i], SourceFile, HeaderFile))
            break;
    }
    fprintf(HeaderFile, "#endif /* %s */", HeaderGuard);

    fclose(SourceFile);
    fclose(HeaderFile);
    return 0;
}


