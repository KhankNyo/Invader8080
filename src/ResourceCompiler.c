
/*
 * takes in a binary file and convert it into a C file containing an array of bytes 
 * */
#include <stdio.h>
#include "File.c"

static void PrintUsage(const char *ProgramName)
{
    printf("Usage: %s "
            "<binary input file> <resource identifier> "
            "<output source file> <output header file>\n", 
        ProgramName
    );
}

int main(int argc, char **argv)
{
    if (argc != 5)
    {
        PrintUsage(argv[0]);
        return 0;
    }
    const char *InputFileName = argv[1];
    const char *ResourceIdentifier = argv[2];
    const char *OutputSourceName = argv[3];
    const char *OutputHeaderName = argv[4];


    FileInfo InputFile = FileRead(InputFileName, false);
    if (NULL == InputFile.Buffer)
    {
        perror(InputFileName);
        return 1;
    }
    FILE *OutputSource = fopen(OutputSourceName, "wb");
    if (NULL == OutputSource)
    {
        fprintf(stderr, "Cannot open %s for writing.\n", OutputSourceName);
        return 1;
    }
    FILE *OutputHeader = fopen(OutputHeaderName, "wb");
    if (NULL == OutputHeader)
    {
        fprintf(stderr, "Cannot open %s for writing.\n", OutputHeaderName);
        return 1;
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
        fprintf(OutputSource, "0x%02x, ", (unsigned)InputFile.Buffer[i]);
    }
    fprintf(OutputSource, 
            "\n"
            "};\n"
    );
    fprintf(OutputSource, "const unsigned long %sSize = sizeof %s;",
        ResourceIdentifier, ResourceIdentifier
    );

    /* header */
    fprintf(OutputHeader, "#ifndef %s_HEADER_GUARD_H\n", ResourceIdentifier);
    fprintf(OutputHeader, "#define %s_HEADER_GUARD_H\n", ResourceIdentifier);
    fprintf(OutputHeader, "extern const unsigned char %s[];\n", ResourceIdentifier);
    fprintf(OutputHeader, "extern const unsigned long %sSize;\n", ResourceIdentifier);
    fprintf(OutputHeader, "#endif /* %s_HEADER_GUARD_H */\n", ResourceIdentifier);


    FileCleanup(&InputFile);
    fclose(OutputSource);
    fclose(OutputHeader);
    return 0;
}


