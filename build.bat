@echo off


set "INCARG=-I.\src\Include\ -I.\resources\"
set "GCC=gcc -Ofast -flto -std=c11 -Wall -Wpedantic -Wextra %INCARG%"
set "MSVC_INC=/I..\src\Include /I..\resources\"
set "MSVC_CL=cl /std:c11 %MSVC_INC%"

if "%1"=="clean" (
    if exist bin rmdir /q /s bin
) else if "%1"=="cl" (
    rem uhhh hopefully people have msvc installed in their C drive 
    if "%VisualStudioVersion%"=="" call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

    if not exist bin\ (
        mkdir bin
        pushd bin
            %MSVC_CL% /FeResourceCompiler.exe ..\src\ResourecCompiler.c
            ResourceCompiler.exe ..\resources\Resources.c ..\resources\Resources.h
        popd
    )

    pushd bin
        %MSVC_CL% /DSTANDALONE /FeDisassembler.exe ..\src\Disassembler.c
        %MSVC_CL% /DSTANDALONE /Fe8080.exe ..\src\8080.c
        %MSVC_CL% /DPLATFORM_WIN32 ^
            /FeInvader.exe ..\src\Win32.c ..\resources\Resources.c ^
            user32.lib kernel32.lib gdi32.lib winmm.lib ^
            /link /entry:wWinMainCRTStartup /nodefaultlib /subsystem:windows 
    popd
) else (
    if not exist bin\ (
        mkdir bin

        %GCC% -o bin\ResourceCompiler.exe src\ResourceCompiler.c
        bin\ResourceCompiler.exe resources\Resources.c resources\Resources.h
    )

    %GCC% -DSTANDALONE -o bin\Disassembler.exe src\Disassembler.c
    %GCC% -DSTANDALONE -o bin\8080.exe src\8080.c
    %GCC% -DPLATFORM_WIN32 ^
        -fno-builtin -nostartfiles -nostdlib -municode^
        -o bin\Invader.exe ^
        src\Win32.c ^
        resources\Resources.c ^
        -Wl,-ewWinMain,-subsystem=windows^
        -luser32 -lkernel32 -lgdi32 -lwinmm
)
