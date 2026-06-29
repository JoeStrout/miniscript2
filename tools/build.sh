#!/bin/bash
# MiniScript2 build orchestration script

set -e  # Exit on any error

PROJECT_ROOT="$(dirname "$0")/.."
cd "$PROJECT_ROOT"

echo "=== MiniScript2 Build Script ==="
echo "Project root: $(pwd)"

# Parse command line arguments
TARGET="${1:-all}"
GOTO_MODE="${2:-auto}"  # auto, on, off

# ---------------------------------------------------------------------------
# dist_build PLATFORM [GOTO_MODE]
#
# Build a distribution binary for PLATFORM using Zig as a cross-compiler.
# PLATFORM is one of: mac-x86, mac-arm, win, linux
# GOTO_MODE is on/off/auto (default: auto → platform-specific safe default)
#
# Output is placed in build/dist/.
# ---------------------------------------------------------------------------
dist_build() {
    local PLATFORM="$1"
    local GOTO_MODE="${2:-auto}"

    # Per-platform settings
    local ZIG_TARGET USE_EDITLINE DEFAULT_GOTO OUTPUT_NAME STRIP
    case "$PLATFORM" in
        mac-x86)
            ZIG_TARGET="-target x86_64-macos"
            USE_EDITLINE=1
            DEFAULT_GOTO="on"
            OUTPUT_NAME="miniscript2-mac-x86"
            STRIP=0
            ;;
        mac-arm)
            ZIG_TARGET="-target aarch64-macos"
            USE_EDITLINE=1
            DEFAULT_GOTO="on"
            OUTPUT_NAME="miniscript2-mac-arm"
            STRIP=0
            ;;
        win)
            ZIG_TARGET="-target x86_64-windows-gnu"
            USE_EDITLINE=0
            DEFAULT_GOTO="on"
            OUTPUT_NAME="miniscript2-win.exe"
            STRIP=1
            ;;
        linux)
            ZIG_TARGET="-target x86_64-linux-musl"
            USE_EDITLINE=1
            DEFAULT_GOTO="on"
            OUTPUT_NAME="miniscript2-linux"
            STRIP=1
            ;;
        *)
            echo "Unknown dist platform: $PLATFORM"
            echo "Valid platforms: mac-x86, mac-arm, win, linux"
            exit 1
            ;;
    esac

    # Resolve goto mode: "auto" picks the platform default
    [ "$GOTO_MODE" = "auto" ] && GOTO_MODE="$DEFAULT_GOTO"
    local GOTO_FLAG
    case "$GOTO_MODE" in
        on)  GOTO_FLAG="-DVM_USE_COMPUTED_GOTO=1" ;;
        off) GOTO_FLAG="-DVM_USE_COMPUTED_GOTO=0" ;;
        *)   GOTO_FLAG="" ;;
    esac

    # Collect sources (exclude test/debug programs)
    local CORE_CPP CORE_C GEN_CPP
    CORE_CPP=$(find cpp/core -maxdepth 1 -name "*.cpp" \
                ! -name "test_*" ! -name "debug_*" | sort)
    CORE_C=$(find cpp/core -maxdepth 1 -name "*.c" \
              ! -name "test_*" ! -name "debug_*" | sort)
    GEN_CPP=$(find generated -maxdepth 1 -name "*.g.cpp" | sort)

    if [ -z "$GEN_CPP" ]; then
        echo "No generated .g.cpp files found. Run 'transpile' first."
        exit 1
    fi

    local EDITLINE_C_SOURCES=""
    local EDITLINE_C_FLAGS="-Icpp/editline"
    local EDITLINE_DEFINE=""
    if [ "$USE_EDITLINE" = "1" ]; then
        EDITLINE_C_SOURCES="cpp/editline/editline.c cpp/editline/sysunix.c cpp/editline/complete.c"
        EDITLINE_DEFINE="-DUSE_EDITLINE=1"
    fi

    local OUTPUT="build/dist/$OUTPUT_NAME"
    local OBJDIR="build/dist/obj_$PLATFORM"
    mkdir -p build/dist "$OBJDIR"

    echo "  Platform : $PLATFORM  ($ZIG_TARGET)"
    echo "  Goto     : $GOTO_MODE"
    echo "  Editline : $USE_EDITLINE"
    echo "  Output   : $OUTPUT"

    local COMMON_FLAGS="$ZIG_TARGET -O3 -DNDEBUG -Wno-date-time -Icpp/core -Icpp -Igenerated $GOTO_FLAG $EDITLINE_DEFINE"
    local OBJECTS=""

    # Compile C files (core + editline)
    for src in $CORE_C $EDITLINE_C_SOURCES; do
        local obj="$OBJDIR/$(basename "${src%.*}").o"
        # shellcheck disable=SC2086
        zig cc $COMMON_FLAGS -std=gnu99 $EDITLINE_C_FLAGS -c "$src" -o "$obj"
        OBJECTS="$OBJECTS $obj"
    done

    # Compile C++ files (core + generated)
    for src in $CORE_CPP $GEN_CPP; do
        local obj="$OBJDIR/$(basename "${src%.cpp}").o"
        # .g.cpp files share a basename pattern; keep the full stem
        obj="$OBJDIR/$(basename "$src" .cpp | tr '.' '_').o"
        # shellcheck disable=SC2086
        zig c++ $COMMON_FLAGS -std=gnu++11 -c "$src" -o "$obj"
        OBJECTS="$OBJECTS $obj"
    done

    # Link
    local STRIP_FLAG=""
    [ "$STRIP" = "1" ] && STRIP_FLAG="-s"
    # shellcheck disable=SC2086
    zig c++ $ZIG_TARGET $STRIP_FLAG $OBJECTS -o "$OUTPUT"

    echo "  Done: $OUTPUT"
}

# ---------------------------------------------------------------------------
# dist_csharp
#
# Build a framework-dependent C# distribution package.
# Output: build/dist/miniscript2-csharp.zip
# ---------------------------------------------------------------------------
dist_csharp() {
    echo "Building C# distribution package..."
    local PUBLISH="build/dist/cs-publish"
    local ZIPDIR="build/dist/cs-zip"
    rm -rf "$PUBLISH" "$ZIPDIR"
    mkdir -p "$PUBLISH" "$ZIPDIR" build/dist

    dotnet publish cs -c Release -o "$PUBLISH" --nologo -v quiet

    cp "$PUBLISH/miniscript2.dll"                    "$ZIPDIR/miniscript2-csharp.dll"
    cp "$PUBLISH/miniscript2.runtimeconfig.json"     "$ZIPDIR/miniscript2-csharp.runtimeconfig.json"
    cp tools/csharp-dist-README.md                   "$ZIPDIR/README.md"

    rm -rf "$PUBLISH"
    (cd "$ZIPDIR" && zip -q "../miniscript2-csharp.zip" \
        miniscript2-csharp.dll \
        miniscript2-csharp.runtimeconfig.json \
        README.md)
    rm -rf "$ZIPDIR"

    echo "  Done: build/dist/miniscript2-csharp.zip"
}

case "$TARGET" in
    "setup")
        echo "Setting up development environment..."
        mkdir -p build/{cs,cpp,temp}
        mkdir -p generated
        echo "Setup complete."
        ;;

    "cs")
        echo "Building C# version..."
        cd cs
        dotnet build MiniScript2.csproj -o ../build/cs/
        echo "C# build complete."
        ;;
    
    "transpile")
        echo "Transpiling C# to C++..."
        mkdir -p generated

        # Check if a specific file was provided
        if [ -n "$2" ]; then
            # Single file transpile
            SINGLE_FILE="$2"
            echo "Transpiling single file: cs/$SINGLE_FILE"
            miniscript tools/transpile.ms -c cs -o generated "cs/$SINGLE_FILE"
            if [ $? -ne 0 ]; then
                echo "Error: Failed to transpile cs/$SINGLE_FILE"
                exit 1
            fi
        else
            # Find all .cs files in the cs directory, excluding build artifacts
            cs_files=$(find cs -name "*.cs" -type f -not -path "*/obj/*" -not -path "*/bin/*")

            if [ -z "$cs_files" ]; then
                echo "No .cs files found in cs/ directory."
                exit 1
            fi

            echo "Found C# files:"
            echo "$cs_files"

            # Transpile all .cs files in a single call
            echo "Transpiling all files..."
            miniscript tools/transpile.ms $cs_files
            if [ $? -ne 0 ]; then
                echo "Error: Failed to transpile C# files"
                exit 1
            fi
        fi

        echo "Transpilation complete."
        ;;
    
    "cpp")
        echo "Building C++ version..."
        if [ ! -f "generated/App.g.cpp" ]; then
            echo "Transpiled C++ files not found. Run 'transpile' first."
            exit 1
        fi

        # Parse cpp sub-arguments: [debug] [goto_mode] [file.cpp]
        BUILD_MODE="release"
        CPP_GOTO_MODE="auto"
        COMPILE_FILE=""
        shift  # consume "cpp"
        for arg in "$@"; do
            case "$arg" in
                "debug")           BUILD_MODE="debug" ;;
                "on"|"off"|"auto") CPP_GOTO_MODE="$arg" ;;
                *.cpp)             COMPILE_FILE="$arg" ;;
            esac
        done

        MAKE_ARGS=""
        if [ "$BUILD_MODE" = "debug" ]; then
            echo "Building in DEBUG mode (-O0 -g, asserts enabled)"
            MAKE_ARGS="$MAKE_ARGS BUILD_MODE=debug"
        else
            echo "Building in RELEASE mode (-O3, asserts disabled)"
        fi

        case "$CPP_GOTO_MODE" in
            "on")
                echo "Forcing computed-goto ON"
                MAKE_ARGS="$MAKE_ARGS GOTO_MODE=on"
                ;;
            "off")
                echo "Forcing computed-goto OFF"
                MAKE_ARGS="$MAKE_ARGS GOTO_MODE=off"
                ;;
            *)
                echo "Using auto-detected computed-goto"
                ;;
        esac

        if [ -n "$COMPILE_FILE" ]; then
            # Compile a single file without linking
            BASENAME="${COMPILE_FILE##*/}"
            if [[ "$BASENAME" == *.g.cpp ]]; then
                STEM="${BASENAME%.g.cpp}"
                OBJ="../build/cpp/obj/gen_${STEM}.o"
            else
                STEM="${BASENAME%.cpp}"
                OBJ="../build/cpp/obj/core_${STEM}.o"
            fi
            echo "Compiling $BASENAME only (no link)..."
            rm -f "$OBJ"
            make -C cpp "$OBJ" $MAKE_ARGS
            echo "Compile of $BASENAME complete."
        else
            make -C cpp $MAKE_ARGS
            echo "C++ build complete."
        fi
        ;;
    
    "all")
        echo "Building all targets..."
        $0 cs
        $0 transpile
        shift  # consume "all"
        $0 cpp "$@"
        echo "All builds complete."
        ;;
    
    "clean")
        echo "Cleaning all build artifacts..."
        rm -rf build/cs/* build/cpp/* build/temp/* build/dist/*
        rm -rf generated/*.g.h generated/*.g.cpp
        cd cs && dotnet clean
        if [ -f cpp/Makefile ]; then
            make -C cpp clean
        fi
        echo "Clean complete."
        ;;
    
    "dist")
        # Distribution builds via Zig cross-compiler
        # Usage: build.sh dist <platform|all> [on|off|auto]
        #   Platforms: mac-x86  mac-arm  mac  win  linux  all
        DIST_PLATFORM="${2:-}"
        DIST_GOTO="${3:-auto}"

        if [ -z "$DIST_PLATFORM" ]; then
            echo "Usage: $0 dist <platform> [goto_mode]"
            echo "Platforms: mac-x86  mac-arm  mac  win  linux  csharp  all"
            exit 1
        fi

        if [ "$DIST_PLATFORM" = "mac" ]; then
            echo "Building Mac fat (universal) binary..."
            dist_build mac-x86 "$DIST_GOTO"
            dist_build mac-arm "$DIST_GOTO"
            lipo -create -output build/dist/miniscript2-mac \
                build/dist/miniscript2-mac-x86 \
                build/dist/miniscript2-mac-arm
            echo "  Fat binary: build/dist/miniscript2-mac"
        elif [ "$DIST_PLATFORM" = "all" ]; then
            echo "Building all distribution targets..."
            dist_build mac-x86 "$DIST_GOTO"
            dist_build mac-arm "$DIST_GOTO"
            lipo -create -output build/dist/miniscript2-mac \
                build/dist/miniscript2-mac-x86 \
                build/dist/miniscript2-mac-arm
            echo "  Fat binary: build/dist/miniscript2-mac"
            dist_build win   "$DIST_GOTO"
            dist_build linux "$DIST_GOTO"
            dist_csharp
            echo "All dist builds complete."
        elif [ "$DIST_PLATFORM" = "csharp" ]; then
            dist_csharp
        else
            dist_build "$DIST_PLATFORM" "$DIST_GOTO"
        fi
        ;;

    "test")
        echo "Running quick smoke tests..."
        echo "Testing C# version:"
        cd build/cs && echo "These are some words for testing" | ./miniscript2
        cd ../..
        echo "Testing C++ version:"
        cd build/cpp && echo "These are some words for testing" | ./miniscript2
        cd ../..
        ;;

    "test-all")
        echo "Running all test suites..."
        make -C tests all
        ;;

    "test-cpp")
        echo "Running C++ test suites..."
        make -C tests cpp
        ;;

    "test-cs")
        echo "Running C# test suites..."
        make -C tests cs
        ;;

    "test-vm")
        echo "Running VM tests..."
        make -C tests vm
        ;;

    "xcode")
        echo "Generating Xcode project..."
        if [ ! -f "cpp/CMakeLists.txt" ]; then
            echo "Error: cpp/CMakeLists.txt not found."
            exit 1
        fi
        mkdir -p cpp/xcode
        cd cpp/xcode
        cmake -G Xcode -DCMAKE_BUILD_TYPE=Debug ..
        echo "Xcode project generated at cpp/xcode/miniscript2.xcodeproj"
        echo "Open with: open cpp/xcode/miniscript2.xcodeproj"
        ;;

    *)
        echo "Usage: $0 {setup|cs|transpile|cpp|dist|all|clean|test|test-*|xcode} [options]"
        echo ""
        echo "Build Commands:"
        echo "  setup       - Set up development environment"
        echo "  cs          - Build C# version only"
        echo "  transpile [file.cs] - Transpile C# to C++ (all files, or single file)"
        echo "  cpp [debug] [goto_mode] [file.cpp] - Build C++ version (or compile single file)"
        echo "  dist <platform> [goto_mode] - Cross-platform dist build via Zig"
        echo "  all         - Build everything (cs + transpile + cpp)"
        echo "  clean       - Clean build artifacts"
        echo ""
        echo "Dist Platforms:"
        echo "  mac-x86     - macOS Intel binary"
        echo "  mac-arm     - macOS Apple Silicon binary"
        echo "  mac         - macOS universal (fat) binary (builds both + lipo)"
        echo "  win         - Windows x86-64 binary (.exe)"
        echo "  linux       - Linux x86-64 static binary (musl)"
        echo "  csharp      - C# fallback build (.zip with .dll + runtimeconfig + README)"
        echo "  all         - All of the above"
        echo ""
        echo "Test Commands:"
        echo "  test        - Quick smoke test of built executables"
        echo "  test-all    - Run all test suites (C++ and C#)"
        echo "  test-cpp    - Run all C++ test suites"
        echo "  test-cs     - Run all C# test suites"
        echo "  test-vm     - Run VM tests only"
        echo ""
        echo "IDE:"
        echo "  xcode       - Generate Xcode project in cpp/xcode/"
        echo ""
        echo "C++ build options (can be combined in any order):"
        echo "  debug       - Build with -O0 -g and asserts enabled"
        echo "  auto        - Auto-detect computed-goto support (default)"
        echo "  on          - Force computed-goto ON"
        echo "  off         - Force computed-goto OFF"
        echo ""
        echo "Examples:"
        echo "  $0 cpp                   # Release build, auto goto"
        echo "  $0 cpp debug             # Debug build, auto goto"
        echo "  $0 cpp debug on          # Debug build, computed-goto forced on"
        echo "  $0 cpp VM.g.cpp          # Compile only VM.g.cpp (no link)"
        echo "  $0 cpp debug value.cpp   # Compile only value.cpp in debug mode"
        echo "  $0 dist mac              # Universal Mac binary (requires lipo)"
        echo "  $0 dist linux            # Linux static binary"
        echo "  $0 dist win off          # Windows binary, switch dispatch"
        echo "  $0 dist all              # All platforms"
        exit 1
        ;;
esac

echo "Build script completed successfully."