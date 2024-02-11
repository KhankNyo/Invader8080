@echo off


set "INCARG=-I.\src\Include\ -I.\resources\"
set "CC=tcc -Oz -Wall -Wpedantic -Wextra %INCARG%"

if "%1"=="clean" (
    if exist bin rmdir /q /s bin
    if "%2"=="resources" rmdir /q /s resources
) else (
    if not exist bin\ mkdir bin
    %CC% -o bin\ResourceCompiler.exe src\ResourceCompiler.c

    if not exist resources\ (
        mkdir resources
        bin\ResourceCompiler.exe ^
            rom\invaders.bin ^
            gSpaceInvadersRom ^
            resources\SpaceInvadersRom.c ^
            resources\SpaceInvadersRom.h
    )

    %CC% -DSTANDALONE -o bin\Disassembler.exe src\Disassembler.c
    %CC% -DSTANDALONE -o bin\8080.exe src\8080.c
    %CC% -DPLATFORM_WIN32 -municode -Wl,-subsystem=windows ^
        -o bin\Win32.exe ^
        src\Win32.c ^
        resources\SpaceInvadersRom.c ^
        -lgdi32
)
