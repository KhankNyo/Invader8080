@echo off


set "INCARG=-I.\src\Include\ -I.\resources\"
set "CC=gcc -Ofast -flto -Wall -Wpedantic -Wextra %INCARG%"

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
    %CC% -DPLATFORM_WIN32 ^
        -fno-builtin -nostartfiles -nostdlib -municode^
        -o bin\Invader.exe ^
        src\Win32.c ^
        resources\Resources.c ^
        -Wl,-ewWinMain,-subsystem=windows^
        -luser32 -lkernel32 -lgdi32 -lwinmm
)
