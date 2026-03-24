# DOOMCRAFT 🎮

## Mac

```bash
brew install sdl2
gcc -O2 -o fps fps.c $(sdl2-config --cflags --libs) -lm
./fps
```

> Don't have Homebrew? Install it from [brew.sh](https://brew.sh)

---

## Windows

```bash
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-SDL2
gcc -O2 -o fps fps.c -I"C:/msys64/ucrt64/include/SDL2" -L"C:/msys64/ucrt64/lib" -lSDL2 -lSDL2main -lm
cp C:/msys64/ucrt64/bin/SDL2.dll .
./fps.exe
```

> Don't have MSYS2? Install it from [msys2.org](https://www.msys2.org), use default path `C:\msys64`, then add `C:\msys64\ucrt64\bin` to your PATH.
