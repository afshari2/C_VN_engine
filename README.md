# C VN Engine

A small SDL2 visual novel engine written in C, aimed at a PC-98-inspired dating-sim style. The engine currently supports backgrounds, character sprites, typewriter dialogue, scene transitions, choices, labels, jumps, and simple story variables for long-term route consequences.

## Requirements

- `gcc`
- `make`

SDL2 can be provided in either of two ways:

- Bundled under `third_party/<platform>/`
- Installed on the system, where the Makefile will try `pkg-config` or the
  existing Homebrew fallback on macOS

See [third_party/README.md](third_party/README.md) for the bundled dependency
layout.

## Build

```sh
make
```

Run:

```sh
./vn
```

Clean:

```sh
make clean
```

To see which compiler/linker settings the Makefile selected:

```sh
make print-config
```

## Bundled SDL

The project is set up so you can copy it to another Windows or Linux computer and
build with `make` if the SDL development files are present under `third_party/`.

Expected folders:

```txt
third_party/windows/
  include/SDL2/
  lib/
  bin/

third_party/linux/
  include/SDL2/
  lib/
```

On Windows, `make` builds `vn.exe` and copies DLLs from
`third_party/windows/bin/` next to it.

On Linux, `make` links bundled libraries with an `$ORIGIN/lib` rpath and copies
`.so` files into a local `lib/` folder.

If bundled SDL files are not present, the Makefile falls back to system SDL.

## Export

The export helper stages the executable with the `assets/` folder, preserving the
relative paths the engine uses for backgrounds, sprites, fonts, and scripts.

```sh
make export-macos
make export-linux
make export-windows
make export-android
make export-package
```

Outputs are written under `dist/` or `build/export/`.

### macOS

```sh
make export-macos
```

Creates:

```txt
dist/macos/A Cat in Girls Dormitory.app
```

This must be built on macOS. For distribution outside your own machine, you will
still need the usual macOS signing/notarization steps and a way to bundle SDL2
frameworks or require SDL2 to be installed.

### Linux

```sh
make export-linux
```

Build this on Linux, or set `CC`, `CFLAGS`, and `LDFLAGS` to a Linux cross
compiler/toolchain. The output is:

```txt
dist/linux/vn-linux/
```

### Windows

```sh
make export-windows
```

Requires a MinGW-w64 toolchain such as `x86_64-w64-mingw32-gcc`. You can override
it with:

```sh
WINDOWS_CC=/path/to/x86_64-w64-mingw32-gcc make export-windows
```

The output is:

```txt
dist/windows/vn-windows/
```

Copy the SDL2, SDL2_ttf, and SDL2_image runtime DLLs into that folder before
shipping it.

### Android

Android is possible, but it is not a direct desktop-style export. SDL2 Android
builds need the Android SDK, NDK, Gradle/Android Studio, the SDL2 Android project,
and Android builds of SDL2_image and SDL2_ttf.

```sh
make export-android
```

Creates a staging folder:

```txt
build/export/android/
```

If you already have an SDL2 Android project, the helper can copy the game files
into it:

```sh
ANDROID_SDL_PROJECT=/path/to/SDL2/android-project make export-android
```

The engine uses SDL's Android asset reader for story files and SDL's per-app
writable storage for save data, so the same script/assets/save logic can work on
Android once the SDL project is built.

## Controls

### Main Menu

- `Up` / `W`: previous menu item
- `Down` / `S`: next menu item
- `Space` / `Enter`: confirm
- Mouse: click a menu item
- `Escape`: quit from the main menu, or return from settings

### In Game

- `Space` / `Enter`: advance dialogue, skip typewriter text, or confirm a choice
- `Up` / `W`: previous choice
- `Down` / `S`: next choice
- `F5`: save to `save.dat`
- `F9`: load from `save.dat`
- `Backspace`: go back to the previous dialogue or choice state
- `P`: open Patreon
- Mouse: click the bottom command strip for `SAVE`, `LOAD`, `BACK`, or `PATREON`
- `Escape`: quit

## Main Menu And Settings

The game starts at a simple PC-98-style main menu:

- `NEW GAME`: restarts the story from the beginning
- `LOAD`: loads the current save slot if it exists
- `SETTINGS`: opens the settings screen
- `PATREON`: opens `https://patreon.com/c10ud`
- `QUIT`: exits the game

Settings currently includes an `AUDIO: ON/OFF` toggle. The project does not have
an audio mixer yet, so this is stored as player preference data for the audio
system to read once music and sound effects are added.

Settings also includes `FULLSCREEN: ON/OFF`. The game still renders internally at
`640x480`; SDL scales that logical canvas to the current window or desktop
fullscreen size while preserving the 4:3 aspect ratio.

## Project Layout

- `main.c`: SDL startup, model/view/controller initialization, main loop
- `model.c` / `model.h`: script loading, labels, branching, variables, route state
- `view.c` / `view.h`: SDL rendering, backgrounds, sprites, textbox, typewriter, choices, transition
- `controller.c` / `controller.h`: keyboard input and model/view synchronization
- `assets/story.txt`: active story script
- `assets/backgrounds/`: PNG scene backgrounds
- `assets/sprites/`: PNG character sprites
- `assets/fonts/`: project font
- `assets/windowbg.avif`, `assets/textbox.bmp`, `assets/heart.png`: UI assets

`script.c` is an older unused parser and is not part of the current build.

## Script Format

The active script is loaded from:

```txt
assets/story.txt
```

Blank lines and lines starting with `#` are ignored.

### Backgrounds

```txt
BG assets/backgrounds/lounge.png
```

Sets the current background. Background changes use the venetian-blind transition.

### Sprites

```txt
SPRITE assets/sprites/emi_smile.png center
SPRITE assets/sprites/sara_stern.png left
SPRITE assets/sprites/sara_soft.png right
```

Supported positions:

- `left`
- `center`
- `right`

### Dialogue

```txt
SAY Emi Oh, you're soaking wet! You poor little thing.
SAY Narrator The hallway is warm and smells like vanilla and old books.
```

Dialogue is shown in the textbox as:

```txt
[Emi] Oh, you're soaking wet! You poor little thing.
```

### Choices

```txt
CHOICE
OPTION Pad over to Emi -> emi_route
OPTION Sit politely in front of Sara -> sara_route
ENDCHOICE
```

Choices display in a PC-98-style choice box. Confirming a choice jumps to the target label.

### Labels And Jumps

```txt
LABEL emi_route
SAY Emi Aww, you chose me?
GOTO common_evening

LABEL common_evening
SAY Narrator Later that night...
```

Labels mark destinations. `GOTO` jumps to a label.

### Route Variables

Variables are integer values stored by the model while the story runs. Undefined variables read as `0`.

```txt
SET emi_affection 0
ADD emi_affection 1
```

Use these for affection points, flags, route counters, and simple dating-sim state.

### Conditional Branches

```txt
IF emi_affection >= 3 GOTO emi_route
IF sara_affection >= 3 GOTO sara_route
GOTO neutral_route
```

Supported operators:

- `==`
- `!=`
- `>=`
- `<=`
- `>`
- `<`

This allows long common routes where earlier decisions affect later scenes or endings.

### Ending The Script

```txt
END
```

Stops script loading at that point.

## Example Route Pattern

```txt
SET emi_affection 0
SET sara_affection 0

LABEL day_1
SAY Narrator The first evening begins.

CHOICE
OPTION Help Emi prepare tea -> help_emi
OPTION Ask Sara about dorm rules -> ask_sara
ENDCHOICE

LABEL help_emi
ADD emi_affection 1
SAY Emi Thanks. You're sweeter than you look.
GOTO day_2

LABEL ask_sara
ADD sara_affection 1
SAY Sara At least someone here respects order.
GOTO day_2

LABEL day_2
SAY Narrator A few days pass.

IF emi_affection >= 1 GOTO emi_scene
IF sara_affection >= 1 GOTO sara_scene
GOTO neutral_scene
```

## Current Limitations

- Only one sprite is displayed at a time.
- Choices support up to `MAX_CHOICES` options, currently `4`.
- Save/load currently uses one slot, `save.dat`, stored in SDL's per-app writable directory.
- Back history is kept in memory for the current run and is cleared after loading a save.
- Text wrapping is simple and space-based.
- The settings menu has an audio preference, but no audio mixer is implemented yet.

## Good Next Features

- Multiple save/load slots
- Multiple sprites on screen
- Music and sound effects
- Character name colors
- Backlog/history screen
- Auto mode and skip mode
- Route debug overlay
- External config for window title, resolution, and starting script
