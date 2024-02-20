## Space Invaders and Intel 8080 emulator
- Written in C with no libraries, excepts for native API
- Was written because I wanted to learn more about audio processing and multithreading stuff
- Included is a disassembler, an i8080 emulator, and the crown jewel, the Space Invader emulator
- Along with tests and sound samples

## Build:
- build with gcc:
```
.\build.bat
```
- build with cl (msvc):
```
.\build.bat cl
```
- clean build binaries:
```
.\build.bat clean
```

## Controls:
- '1', '2': player 1 select, player 2 select
- 'r': reset
- 't': tilt
- 'c': insert coin
- 'a', 'd': player 1 move left, right 
- left arrow, right arrow: player 2 move left, right
- space: player 1 shoots
- enter: player 2 shoots

## Issues and TODO's:
- When the UFO appears on screen, its sound loop has a pulsing beat that should not be there.
- Platform_CreateCriticalSection is currently very hacky.
- SleepTime variable in Win32_SoundThreadRoutine should be tuned more carefully (sweet spot is probably around .1 to .3) so that the sound thread stays responsive when audio device needs more sample while not using 100% of a core
