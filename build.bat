@echo off


set "INCDIR=.\src\Include\"
if "%1"=="clean" (
    if exist bin rmdir /q /s bin
) else (
    if not exist bin\ mkdir bin
    tcc -I%INCDIR% -DSTANDALONE -o bin\Disassembler.exe src\Disassembler.c
    tcc -I%INCDIR% -DSTANDALONE -o bin\Emulator.exe src\Emulator.c
)
