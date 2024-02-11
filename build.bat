@echo off


set "INCARG=-I.\src\Include\ -I.\resources\"
set "CC=gcc -Oz -Wall -Wpedantic -Wextra %INCARG%"

if "%1"=="clean" (
    if exist bin rmdir /q /s bin
) else (
    if not exist bin\ (
        mkdir bin

        %CC% -o bin\ResourceCompiler.exe src\ResourceCompiler.c
        bin\ResourceCompiler.exe resources\Resources.c resources\Resources.h
    )

    %CC% -DSTANDALONE -o bin\Disassembler.exe src\Disassembler.c
    %CC% -DSTANDALONE -o bin\8080.exe src\8080.c
    %CC% -DPLATFORM_WIN32 -municode ^
        -o bin\Win32.exe ^
        src\Win32.c ^
        resources\Resources.c ^
        -lgdi32 -lwinmm
)
