@echo off


set "INCDIR=.\src\Include\"
set "CC=gcc -O0 -ggdb -Wall -Wpedantic -Wextra"
if "%1"=="clean" (
    if exist bin rmdir /q /s bin
) else (
    if not exist bin\ mkdir bin
    %CC% -I%INCDIR% -DSTANDALONE -o bin\Disassembler.exe src\Disassembler.c
    %CC% -I%INCDIR% -DSTANDALONE -o bin\8080.exe src\8080.c
    %CC% -I%INCDIR% -DPLATFORM_WIN32 -municode -o bin\Win32.exe src\Win32.c -lgdi32

    copy rom bin
)
