@echo off

if "%1"=="clean" (
    if exist bin rmdir /q /s bin
) else (
    if not exist bin\ mkdir bin
    tcc Disassembler.c
)
