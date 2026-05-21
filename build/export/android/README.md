# Android Export Staging

This folder contains the game sources and assets staged for an SDL2 Android build.

Android is different from the desktop targets: SDL2 supplies the Java Activity,
Gradle project, and native library glue. This engine now uses SDL_RWops for the
story file on Android, so assets inside the APK can be loaded by relative path.

Recommended flow:

1. Download or install the SDL2 source package plus SDL2_image and SDL2_ttf.
2. Use SDL2's `android-project` as the base Gradle project.
3. Put this folder's `jni/src` files into the project's native source area.
4. Put this folder's `assets` directory under `app/src/main/assets/assets`,
   preserving paths such as `assets/script/scene01.txt`.
5. Build with Android Studio or Gradle/NDK.

Shortcut:

```sh
ANDROID_SDL_PROJECT=/path/to/SDL2/android-project tools/export.sh android
```

That copies the staged files into the SDL Android project layout for you.
