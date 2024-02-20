# Space Invaders and Intel 8080 emulator
- Written in C with no libraries, excepts for native API
- Was written because I wanted to learn more about audio processing and multithreading stuff
- Included is a disassembler, an i8080 emulator, and the crown jewel, the Space Invader emulator
- Along with tests and sound samples

# Build:
### With GCC: 
```
.\build.bat
```
### With cl (msvc compiler):
```
.\build.bat cl
```
### Remove built binaries:
```
.\build.bat clean
```

# Controls:
### Miscellaneous:
- 't': tilt
- 'c': insert coin
- 'r': reset
### Player 1:
- '1': select
- 'a': move left
- 'd': move right
- space: shoot
### Player 2:
- '2': select
- left arrow: move left
- right arrow: move right
- enter: shoot

# Issues and TODO's:
- When the UFO appears on screen, its sound loop has a pulsing beat that should not be there.
- When there are only a few Invaders left, the Invader marching sound is very uneven (timing issues).
- Platform_CreateCriticalSection is currently very hacky.
- SleepTime variable in Win32_SoundThreadRoutine should be tuned more carefully (sweet spot is probably around .1 to .3) to keep the the sound thread responsive while not using 100% of a core. Though this appears to be cpu dependent (used 5~10% on an i5-1235u, always 30% on an i5-7400 according to Task Manager).
- Game might stutter a lot on something that has a bunch of efficiency cores (noticeable stutters on an i5-1235u, but none on an i5-7400)
- Port this to Linux and MacOS
