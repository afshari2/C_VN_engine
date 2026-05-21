# Third-Party SDL Dependencies

This folder is for bundling SDL development files with the project so a Windows
or Linux machine with `gcc` can build with `make` without installing SDL
separately.

The Makefile automatically prefers bundled files here when it finds:

```txt
third_party/<platform>/include/SDL2/SDL.h
```

Supported platform folders:

```txt
third_party/windows/
third_party/linux/
```

## Windows Layout

Use MinGW-w64 development packages for SDL2, SDL2_image, and SDL2_ttf.

Expected layout:

```txt
third_party/windows/
  include/
    SDL2/
      SDL.h
      SDL_image.h
      SDL_ttf.h
      ...
  lib/
    libSDL2.dll.a
    libSDL2main.a
    libSDL2_image.dll.a
    libSDL2_ttf.dll.a
    ...
  bin/
    SDL2.dll
    SDL2_image.dll
    SDL2_ttf.dll
    ...
```

On Windows, `make` builds `vn.exe` and copies DLLs from
`third_party/windows/bin/` next to it.

## Linux Layout

Linux shared libraries are less portable across distributions than Windows DLLs.
For the most reliable Linux workflow, install distro SDL development packages and
let the Makefile use `pkg-config`.

If you still want bundled Linux libraries, use this layout:

```txt
third_party/linux/
  include/
    SDL2/
      SDL.h
      SDL_image.h
      SDL_ttf.h
      ...
  lib/
    libSDL2.so
    libSDL2_image.so
    libSDL2_ttf.so
    ...
```

On Linux, `make` links with an rpath of `$ORIGIN/lib` and copies bundled `.so`
files into a local `lib/` folder.

## macOS

macOS still defaults to Homebrew or `pkg-config`. A vendored macOS layout can be
added later, but app bundling/signing needs separate handling.
