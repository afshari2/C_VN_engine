#!/usr/bin/env sh
set -eu

APP_NAME=${APP_NAME:-A Cat in Girls Dormitory}
APP_ID=${APP_ID:-com.example.cvnengine}
OUT_NAME=${OUT_NAME:-vn}
SRC="main.c model.c view.c controller.c"
ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
EXPORT_DIR="$ROOT_DIR/build/export"
DIST_DIR="$ROOT_DIR/dist"

usage() {
    printf '%s\n' "Usage: tools/export.sh [macos|linux|windows|android|all|package]"
    printf '%s\n' ""
    printf '%s\n' "Environment overrides:"
    printf '%s\n' "  CC, CFLAGS, LDFLAGS          desktop compiler settings"
    printf '%s\n' "  WINDOWS_CC                   Windows cross compiler, default x86_64-w64-mingw32-gcc"
    printf '%s\n' "  ANDROID_SDL_PROJECT          optional SDL Android project to populate"
    printf '%s\n' "  APP_NAME, APP_ID, OUT_NAME   package metadata"
}

require_cmd() {
    if ! command -v "$1" >/dev/null 2>&1; then
        printf '%s\n' "Missing required command: $1" >&2
        return 1
    fi
}

copy_assets() {
    dest=$1
    mkdir -p "$dest/assets"
    (cd "$ROOT_DIR/assets" && tar --exclude='.DS_Store' -cf - .) |
        (cd "$dest/assets" && tar -xf -)
}

copy_sources() {
    dest=$1
    mkdir -p "$dest"
    for file in $SRC model.h view.h controller.h; do
        cp "$ROOT_DIR/$file" "$dest/"
    done
}

desktop_flags() {
    if command -v pkg-config >/dev/null 2>&1 &&
       pkg-config --exists sdl2 SDL2_ttf SDL2_image; then
        CFLAGS=${CFLAGS:-$(pkg-config --cflags sdl2 SDL2_ttf SDL2_image)}
        LDFLAGS=${LDFLAGS:-$(pkg-config --libs sdl2 SDL2_ttf SDL2_image)}
    elif [ "$(uname -s)" = "Darwin" ]; then
        CFLAGS=${CFLAGS:-"-I/opt/homebrew/include -Wall"}
        LDFLAGS=${LDFLAGS:-"-L/opt/homebrew/lib -lSDL2 -lSDL2_ttf -lSDL2_image"}
    else
        CFLAGS=${CFLAGS:-"-Wall"}
        LDFLAGS=${LDFLAGS:-"-lSDL2 -lSDL2_ttf -lSDL2_image"}
    fi
}

build_desktop() {
    target=$1
    exe=$2
    cc=${CC:-gcc}

    desktop_flags
    mkdir -p "$EXPORT_DIR/$target"
    cd "$ROOT_DIR"
    # Intentionally unquoted flags allow callers to pass normal compiler flag lists.
    $cc $SRC -o "$EXPORT_DIR/$target/$exe" $CFLAGS $LDFLAGS
}

export_macos() {
    if [ "$(uname -s)" != "Darwin" ]; then
        printf '%s\n' "macOS export must be built on macOS." >&2
        return 1
    fi

    app="$DIST_DIR/macos/$APP_NAME.app"
    rm -rf "$app"
    mkdir -p "$app/Contents/MacOS"
    mkdir -p "$app/Contents/Resources"

    build_desktop macos "$OUT_NAME"
    cp "$EXPORT_DIR/macos/$OUT_NAME" "$app/Contents/MacOS/$OUT_NAME"
    copy_assets "$app/Contents/Resources"
    bundle_macos_dylibs "$app" "$app/Contents/MacOS/$OUT_NAME"

    cat > "$app/Contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
 "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleExecutable</key><string>$OUT_NAME</string>
  <key>CFBundleIdentifier</key><string>$APP_ID</string>
  <key>CFBundleName</key><string>$APP_NAME</string>
  <key>CFBundlePackageType</key><string>APPL</string>
  <key>CFBundleVersion</key><string>1.0.0</string>
  <key>CFBundleShortVersionString</key><string>1.0.0</string>
  <key>NSHighResolutionCapable</key><true/>
</dict>
</plist>
EOF

    if command -v codesign >/dev/null 2>&1; then
        codesign --force --deep --sign - "$app" >/dev/null 2>&1 || true
    fi

    printf '%s\n' "Created $app"
}

is_macos_system_dylib() {
    case "$1" in
        /System/*|/usr/lib/*) return 0 ;;
        *) return 1 ;;
    esac
}

resolve_macos_dylib() {
    dep=$1
    loader_dir=$2

    case "$dep" in
        @executable_path/*|@loader_path/*)
            return 1
            ;;
        @rpath/*)
            name=$(basename "$dep")
            for dir in "$loader_dir" /opt/homebrew/lib /opt/homebrew/opt/*/lib /usr/local/lib /usr/local/opt/*/lib; do
                if [ -f "$dir/$name" ]; then
                    printf '%s\n' "$dir/$name"
                    return 0
                fi
            done
            return 1
            ;;
        *)
            if [ -f "$dep" ]; then
                printf '%s\n' "$dep"
                return 0
            fi
            return 1
            ;;
    esac
}

copy_macos_dylib_tree() {
    binary=$1
    frameworks=$2
    copied=$3
    loader_dir=$(dirname "$binary")

    otool -L "$binary" | awk 'NR > 1 {print $1}' | while read -r dep; do
        [ -n "$dep" ] || continue
        is_macos_system_dylib "$dep" && continue
        resolved=$(resolve_macos_dylib "$dep" "$loader_dir" || true)
        [ -n "$resolved" ] || continue

        name=$(basename "$resolved")
        dest="$frameworks/$name"
        if ! grep -qx "$dest" "$copied" 2>/dev/null; then
            cp "$resolved" "$dest"
            chmod u+w "$dest"
            printf '%s\n' "$dest" >> "$copied"
            copy_macos_dylib_tree "$dest" "$frameworks" "$copied"
        fi
    done
}

rewrite_macos_dylibs() {
    binary=$1
    frameworks=$2

    if [ "$(basename "$binary")" != "$OUT_NAME" ]; then
        install_name_tool -id "@executable_path/../Frameworks/$(basename "$binary")" "$binary" 2>/dev/null || true
    fi

    otool -L "$binary" | awk 'NR > 1 {print $1}' | while read -r dep; do
        [ -n "$dep" ] || continue
        is_macos_system_dylib "$dep" && continue
        name=$(basename "$dep")
        if [ -f "$frameworks/$name" ]; then
            install_name_tool -change "$dep" "@executable_path/../Frameworks/$name" "$binary" 2>/dev/null || true
        fi
    done
}

bundle_macos_dylibs() {
    app=$1
    exe=$2
    frameworks="$app/Contents/Frameworks"
    copied="$EXPORT_DIR/macos/copied-dylibs.txt"

    require_cmd otool
    require_cmd install_name_tool

    mkdir -p "$frameworks"
    : > "$copied"

    copy_macos_dylib_tree "$exe" "$frameworks" "$copied"

    rewrite_macos_dylibs "$exe" "$frameworks"
    find "$frameworks" -type f -name '*.dylib' | while read -r dylib; do
        rewrite_macos_dylibs "$dylib" "$frameworks"
    done
}

export_linux() {
    if [ "$(uname -s)" != "Linux" ] && [ -z "${CC+x}" ]; then
        printf '%s\n' "Linux export must be built on Linux, or with CC set to a Linux cross compiler." >&2
        return 1
    fi

    bundle="$DIST_DIR/linux/$OUT_NAME-linux"
    rm -rf "$bundle"
    mkdir -p "$bundle"

    build_desktop linux "$OUT_NAME"
    cp "$EXPORT_DIR/linux/$OUT_NAME" "$bundle/$OUT_NAME"
    copy_assets "$bundle"
    cat > "$bundle/run.sh" <<EOF
#!/usr/bin/env sh
cd "\$(dirname "\$0")"
./$OUT_NAME
EOF
    chmod +x "$bundle/run.sh"
    printf '%s\n' "Created $bundle"
}

export_windows() {
    cc=${WINDOWS_CC:-x86_64-w64-mingw32-gcc}
    require_cmd "$cc"

    bundle="$DIST_DIR/windows/$OUT_NAME-windows"
    rm -rf "$bundle"
    mkdir -p "$bundle"

    CC=$cc CFLAGS=${CFLAGS:-"-Wall"} \
        LDFLAGS=${LDFLAGS:-"-lmingw32 -lSDL2main -lSDL2 -lSDL2_ttf -lSDL2_image"} \
        build_desktop windows "$OUT_NAME.exe"
    cp "$EXPORT_DIR/windows/$OUT_NAME.exe" "$bundle/$OUT_NAME.exe"
    copy_assets "$bundle"
    printf '%s\n' "Created $bundle"
    printf '%s\n' "Copy the SDL2, SDL2_ttf, and SDL2_image runtime DLLs into this folder before shipping."
}

write_android_readme() {
    dest=$1
    cat > "$dest/README.md" <<EOF
# Android Export Staging

This folder contains the game sources and assets staged for an SDL2 Android build.

Android is different from the desktop targets: SDL2 supplies the Java Activity,
Gradle project, and native library glue. This engine now uses SDL_RWops for the
story file on Android, so assets inside the APK can be loaded by relative path.

Recommended flow:

1. Download or install the SDL2 source package plus SDL2_image and SDL2_ttf.
2. Use SDL2's \`android-project\` as the base Gradle project.
3. Put this folder's \`jni/src\` files into the project's native source area.
4. Put this folder's \`assets\` directory under \`app/src/main/assets/assets\`,
   preserving paths such as \`assets/story.txt\`.
5. Build with Android Studio or Gradle/NDK.

Shortcut:

\`\`\`sh
ANDROID_SDL_PROJECT=/path/to/SDL2/android-project tools/export.sh android
\`\`\`

That copies the staged files into the SDL Android project layout for you.
EOF
}

export_android() {
    stage="$EXPORT_DIR/android"
    rm -rf "$stage"
    mkdir -p "$stage/jni/src"
    copy_sources "$stage/jni/src"
    copy_assets "$stage"

    cat > "$stage/jni/Android.mk" <<EOF
LOCAL_PATH := \$(call my-dir)

include \$(CLEAR_VARS)

LOCAL_MODULE := main
LOCAL_SRC_FILES := src/main.c src/model.c src/view.c src/controller.c
LOCAL_SHARED_LIBRARIES := SDL2 SDL2_image SDL2_ttf
LOCAL_CFLAGS := -Wall
LOCAL_LDLIBS := -llog -landroid

include \$(BUILD_SHARED_LIBRARY)
EOF

    cat > "$stage/jni/Application.mk" <<EOF
APP_ABI := arm64-v8a armeabi-v7a x86_64
APP_PLATFORM := android-23
APP_STL := c++_shared
EOF

    write_android_readme "$stage"

    if [ -n "${ANDROID_SDL_PROJECT:-}" ]; then
        mkdir -p "$ANDROID_SDL_PROJECT/app/src/main/assets"
        mkdir -p "$ANDROID_SDL_PROJECT/app/jni/src"
        cp -R "$stage/assets" "$ANDROID_SDL_PROJECT/app/src/main/assets/"
        cp "$stage/jni/src/"* "$ANDROID_SDL_PROJECT/app/jni/src/"
        cp "$stage/jni/Android.mk" "$ANDROID_SDL_PROJECT/app/jni/"
        cp "$stage/jni/Application.mk" "$ANDROID_SDL_PROJECT/app/jni/"
        printf '%s\n' "Populated $ANDROID_SDL_PROJECT"
    else
        printf '%s\n' "Created $stage"
        printf '%s\n' "Set ANDROID_SDL_PROJECT to copy into an SDL2 Android project."
    fi
}

export_package() {
    bundle="$DIST_DIR/source/$OUT_NAME-source"
    rm -rf "$bundle"
    mkdir -p "$bundle"
    copy_sources "$bundle"
    copy_assets "$bundle"
    if [ -d "$ROOT_DIR/third_party" ]; then
        cp -R "$ROOT_DIR/third_party" "$bundle/"
    fi
    mkdir -p "$bundle/tools"
    cp "$ROOT_DIR/tools/export.sh" "$bundle/tools/"
    chmod +x "$bundle/tools/export.sh"
    cp "$ROOT_DIR/Makefile" "$bundle/"
    cp "$ROOT_DIR/README.md" "$bundle/"
    printf '%s\n' "Created $bundle"
}

target=${1:-}
case "$target" in
    macos) export_macos ;;
    linux) export_linux ;;
    windows) export_windows ;;
    android) export_android ;;
    package) export_package ;;
    all) export_package; export_macos; export_linux; export_android ;;
    ""|-h|--help) usage ;;
    *) usage; exit 1 ;;
esac
