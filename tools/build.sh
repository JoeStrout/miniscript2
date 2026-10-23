#!/bin/bash
# MiniScript2 build orchestration script

set -e  # Exit on any error

PROJECT_ROOT="$(dirname "$0")/.."
cd "$PROJECT_ROOT"

echo "=== MiniScript2 Build Script ==="
echo "Project root: $(pwd)"

# Always name the C# project explicitly.  The cs/ folder also holds a solution
# file (and editors may drop more alongside it), so pointing dotnet at the
# folder is ambiguous and fails with MSB1050.
CSPROJ="cs/MiniScript2.csproj"
if [ ! -f "$CSPROJ" ]; then
    echo "Error: C# project not found at $CSPROJ" >&2
    exit 1
fi

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
            OUTPUT_NAME="miniscript-mac-x86"
            STRIP=0
            ;;
        mac-arm)
            ZIG_TARGET="-target aarch64-macos"
            USE_EDITLINE=1
            DEFAULT_GOTO="on"
            OUTPUT_NAME="miniscript-mac-arm"
            STRIP=0
            ;;
        win)
            ZIG_TARGET="-target x86_64-windows-gnu"
            USE_EDITLINE=0
            DEFAULT_GOTO="on"
            OUTPUT_NAME="miniscript-win.exe"
            STRIP=1
            ;;
        linux)
            ZIG_TARGET="-target x86_64-linux-musl"
            USE_EDITLINE=1
            DEFAULT_GOTO="on"
            OUTPUT_NAME="miniscript-linux"
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
# source_fingerprint
#
# A content hash of every source file a dist artifact is built from.  This is
# what lets push_release tell "these binaries are current" from "these binaries
# are old" without consulting timestamps, which lie in both directions: mtimes
# survive a rebase unchanged, and a commit made after a build is newer than the
# binaries it describes even though it holds exactly their source.
#
# obj/ and bin/ are pruned to match push_release's own file scan: dotnet
# publish writes generated .cs files there during 'dist csharp'.
source_fingerprint() {
    find cs cpp generated \
        \( -type d \( -name obj -o -name bin \) -prune \) -o \
        -type f \( -name '*.cs' -o -name '*.c' -o -name '*.h' -o -name '*.cpp' \) \
        -print 2>/dev/null \
    | LC_ALL=C sort \
    | xargs shasum \
    | shasum \
    | cut -d' ' -f1
}

# record_build_stamp
#
# Write the provenance of whatever is now in build/dist: the source
# fingerprint, the commit it was built from, and whether the tree was dirty at
# the time.  Read back by tools/push_release.
record_build_stamp() {
    mkdir -p build/dist
    {
        echo "fingerprint $(source_fingerprint)"
        echo "commit $(git rev-parse HEAD 2>/dev/null || echo unknown)"
        if [ -n "$(git status --porcelain 2>/dev/null)" ]; then
            echo "tree dirty"
        else
            echo "tree clean"
        fi
        echo "built $(date '+%Y-%m-%d %H:%M:%S')"
    } > build/dist/.build-info
    echo "  Recorded build stamp: build/dist/.build-info"
}

# dist_csharp
#
# Build the framework-dependent C# distribution files.
# Output: build/dist/csharp/ (bundled up by dist_package csharp)
# ---------------------------------------------------------------------------
dist_csharp() {
    echo "Building C# distribution files..."
    local PUBLISH="build/dist/cs-publish"
    local OUTDIR="build/dist/csharp"
    rm -rf "$PUBLISH" "$OUTDIR"
    mkdir -p "$PUBLISH" "$OUTDIR"

    dotnet publish "$CSPROJ" -c Release -o "$PUBLISH" --nologo -v quiet

    cp "$PUBLISH/miniscript.dll"                    "$OUTDIR/miniscript-csharp.dll"
    cp "$PUBLISH/miniscript.runtimeconfig.json"     "$OUTDIR/miniscript-csharp.runtimeconfig.json"
    rm -rf "$PUBLISH"

    echo "  Done: $OUTDIR"
}

# ---------------------------------------------------------------------------
# Mac code signing and notarization
#
# Both are opt-in, configured by tools/signing.secret.  That file is
# gitignored, so no identity or credential lives in the repository.  It holds
# KEY=value lines ('#' starts a comment line; quotes around a value are
# optional):
#
#   SIGN_ID=Developer ID Application: Your Name (TEAMID)
#   APPLE_ID=you@example.com
#   APP_PASSWORD=abcd-efgh-ijkl-mnop
#
# SIGN_ID is the identity to sign with (list yours with:
# security find-identity -v -p codesigning); the team ID notarytool needs is
# the part in parentheses.  APPLE_ID and APP_PASSWORD are the Apple account
# and an app-specific password for it (made at appleid.apple.com), used to
# notarize.  With no SIGN_ID, the Mac binary ships as zig leaves it: the
# arm64 slice ad-hoc signed, which is enough to run, but Gatekeeper blocks
# the download.  With SIGN_ID alone, it is signed but not notarized.
#
# A bare executable cannot have its notarization ticket stapled, so we notarize
# the zip instead; Gatekeeper then looks the ticket up online on first launch.
# A successful notarization is recorded in build/dist/.notarized as the hash of
# the zip it covers, which push_release checks before uploading.
# ---------------------------------------------------------------------------
SIGNING_FILE="tools/signing.secret"
NOTARIZED_STAMP="build/dist/.notarized"

# Read $SIGNING_FILE (once) into SIGN_ID, APPLE_ID, APP_PASSWORD and TEAM_ID.
# The file is parsed rather than sourced, so it is data, never code.
load_signing_config() {
    [ -z "${SIGNING_LOADED:-}" ] || return 0
    SIGNING_LOADED=1
    SIGN_ID="" APPLE_ID="" APP_PASSWORD="" TEAM_ID=""
    [ -f "$SIGNING_FILE" ] || return 0

    if [ "$(stat -f %Lp "$SIGNING_FILE")" != "600" ]; then
        echo "  WARNING: $SIGNING_FILE holds a password; make it private with:"
        echo "    chmod 600 $SIGNING_FILE"
    fi

    local key val
    while IFS='=' read -r key val || [ -n "$key" ]; do
        key="${key//[[:space:]]/}"
        case "$key" in ''|'#'*) continue ;; esac
        val="${val%$'\r'}"
        val="${val#"${val%%[![:space:]]*}"}"   # trim leading space
        val="${val%"${val##*[![:space:]]}"}"   # trim trailing space
        val="${val#\"}"; val="${val%\"}"
        case "$key" in
            SIGN_ID)      SIGN_ID="$val" ;;
            APPLE_ID)     APPLE_ID="$val" ;;
            APP_PASSWORD) APP_PASSWORD="$val" ;;
            *)            echo "  $SIGNING_FILE: unknown setting '$key' (ignored)" ;;
        esac
    done < "$SIGNING_FILE"
    TEAM_ID=$(echo "$SIGN_ID" | sed -n 's/.*(\([A-Z0-9]*\))$/\1/p')
}

mac_sign() {
    local BIN="$1"
    load_signing_config
    if [ -z "$SIGN_ID" ]; then
        echo "  No SIGN_ID in $SIGNING_FILE; Mac binary left unsigned."
        return
    fi
    echo "  Signing $BIN as $SIGN_ID ..."
    # --options runtime (the hardened runtime) and --timestamp are both
    # required for notarization.  codesign signs each slice of a fat binary.
    codesign --force --timestamp --options runtime \
        --identifier org.miniscript.miniscript \
        --sign "$SIGN_ID" "$BIN"
    codesign --verify --strict --verbose=2 "$BIN"
}

mac_notarize() {
    local ZIP="$1"
    rm -f "$NOTARIZED_STAMP"
    load_signing_config
    [ -n "$SIGN_ID" ] || return 0
    if [ -z "$APPLE_ID" ] || [ -z "$APP_PASSWORD" ]; then
        echo "  No APPLE_ID and APP_PASSWORD in $SIGNING_FILE; $ZIP is signed but not notarized."
        return
    fi
    if [ -z "$TEAM_ID" ]; then
        echo "  SIGN_ID does not end in (TEAMID); cannot notarize $ZIP."
        return
    fi
    echo "  Submitting $ZIP for notarization (usually a few minutes)..."
    local OUT
    OUT=$(xcrun notarytool submit "$ZIP" --apple-id "$APPLE_ID" --team-id "$TEAM_ID" \
        --password "$APP_PASSWORD" --wait 2>&1) || true
    echo "$OUT" | sed 's/^/    /'
    # notarytool's exit status has not always reflected the verdict, so read
    # the verdict itself.  A rejection is reported but does not stop the
    # other platforms from packaging; push_release will refuse the result.
    if echo "$OUT" | grep -q "status: Accepted"; then
        echo "zip $(shasum "$ZIP" | cut -d' ' -f1)" > "$NOTARIZED_STAMP"
        echo "  Notarized: $ZIP"
    else
        local ID
        ID=$(echo "$OUT" | awk '$1=="id:"{print $2; exit}')
        echo "  NOTARIZATION FAILED for $ZIP."
        # A request refused outright (bad credentials, an unsigned agreement)
        # never becomes a submission, so there is no log to point at; the
        # error printed above is all Apple says.
        if [ -n "$ID" ]; then
            echo "  For Apple's explanation, run:"
            echo "    xcrun notarytool log $ID --apple-id $APPLE_ID --team-id $TEAM_ID"
            echo "  (it will ask for the app-specific password)."
        fi
    fi
}

# ---------------------------------------------------------------------------
# dist_package PLATFORM
#
# Bundle a dist build into the archive users download.  PLATFORM is one of
# mac, win, linux, csharp.  Each archive unpacks to a single folder holding
# the executable, a README, the docs, and the lib folder.  lib sits right
# next to the executable because $MS_EXE_DIR/lib is on the default import
# path (see kDefaultImportPath in ShellIntrinsics.cs), so imports work with
# no setup.
#
#   mac    -> build/dist/miniscript-mac.zip
#   win    -> build/dist/miniscript-win.zip
#   linux  -> build/dist/miniscript-linux.tar.gz
#   csharp -> build/dist/miniscript-csharp.zip
#
# The docs are not built here: MiniScript-Manual.pdf is exported by hand
# from Pages, and the others are rendered from Markdown with tools/md2pdf.sh.
# A missing doc is reported and left out; push_release refuses to publish
# without them.
# ---------------------------------------------------------------------------
DIST_DOCS="docs/MiniScript-Manual.pdf docs/CommandLine.pdf docs/NewInMS2.pdf"

dist_package() {
    local PLATFORM="$1"
    local NAME="miniscript-$PLATFORM"
    local STAGE="build/dist/pkg/$NAME"
    local LABEL RUN REQUIREMENTS="" EXE_LIST
    echo "Packaging $NAME..."
    rm -rf "$STAGE"
    mkdir -p "$STAGE"

    case "$PLATFORM" in
        mac|linux)
            [ -f "build/dist/$NAME" ] || { echo "  Missing build/dist/$NAME; not packaged."; rm -rf "$STAGE"; return; }
            cp "build/dist/$NAME" "$STAGE/miniscript"
            chmod 755 "$STAGE/miniscript"
            if [ "$PLATFORM" = "mac" ]; then mac_sign "$STAGE/miniscript"; fi
            if [ "$PLATFORM" = "mac" ]; then LABEL="macOS"; else LABEL="Linux"; fi
            RUN="./miniscript"
            EXE_LIST="  miniscript               The MiniScript command-line interpreter"
            ;;
        win)
            [ -f "build/dist/$NAME.exe" ] || { echo "  Missing build/dist/$NAME.exe; not packaged."; rm -rf "$STAGE"; return; }
            cp "build/dist/$NAME.exe" "$STAGE/miniscript.exe"
            LABEL="Windows"
            RUN=".\\miniscript"
            EXE_LIST="  miniscript.exe           The MiniScript command-line interpreter"
            ;;
        csharp)
            [ -d build/dist/csharp ] || { echo "  Missing build/dist/csharp; not packaged."; rm -rf "$STAGE"; return; }
            cp build/dist/csharp/* "$STAGE/"
            LABEL=".NET"
            RUN="dotnet miniscript-csharp.dll"
            EXE_LIST="  miniscript-csharp.dll    The MiniScript interpreter (.NET assembly)
  miniscript-csharp.runtimeconfig.json
                           Runtime configuration required by .NET"
            REQUIREMENTS="
Requirements
------------

This is a framework-dependent .NET build, intended as a fallback for
platforms not covered by the native builds (e.g. ARM Linux, FreeBSD).
The native builds are faster and self-contained; prefer them where
available.  This build needs .NET 6 or later, from:

    https://dotnet.microsoft.com/download
"
            ;;
        *)
            echo "Unknown package platform: $PLATFORM"
            exit 1
            ;;
    esac

    # lib, minus the maintainer's sync script and any Finder droppings.
    cp -R lib "$STAGE/lib"
    rm -f "$STAGE/lib/sync_from_sysdisk.sh"
    find "$STAGE/lib" -name .DS_Store -delete

    local doc
    for doc in $DIST_DOCS; do
        if [ -f "$doc" ]; then
            cp "$doc" "$STAGE/"
        else
            echo "  WARNING: $doc not found; left out of $NAME."
        fi
    done
    cp LICENSE "$STAGE/LICENSE"

    # Values go in through the environment rather than awk -v, which would
    # treat the backslash in .\miniscript as an escape.
    LABEL="$LABEL" RUN="$RUN" EXE_LIST="$EXE_LIST" REQUIREMENTS="$REQUIREMENTS" awk '
        function subst(line, key, val,   i) {
            while ((i = index(line, key)) > 0)
                line = substr(line, 1, i - 1) val substr(line, i + length(key))
            return line
        }
        $0 == "@EXE_LIST@"     { print ENVIRON["EXE_LIST"]; next }
        $0 == "@REQUIREMENTS@" { print ENVIRON["REQUIREMENTS"]; next }
        { print subst(subst($0, "@PLATFORM@", ENVIRON["LABEL"]), "@RUN@", ENVIRON["RUN"]) }
    ' tools/dist-README.txt > "$STAGE/README.txt"

    # COPYFILE_DISABLE keeps macOS tar from adding ._ resource-fork files;
    # zip -X leaves out extended attributes for the same reason.
    if [ "$PLATFORM" = "linux" ]; then
        rm -f "build/dist/$NAME.tar.gz"
        (cd build/dist/pkg && COPYFILE_DISABLE=1 tar -czf "../$NAME.tar.gz" "$NAME")
        echo "  Done: build/dist/$NAME.tar.gz"
    else
        rm -f "build/dist/$NAME.zip"
        (cd build/dist/pkg && zip -qrX "../$NAME.zip" "$NAME")
        echo "  Done: build/dist/$NAME.zip"
        if [ "$PLATFORM" = "mac" ]; then mac_notarize "build/dist/$NAME.zip"; fi
    fi
    rm -rf "$STAGE"
}

# ---------------------------------------------------------------------------
# refresh_compile_commands
#
# Keep compile_commands.json (used by clangd) in sync with the source tree.
#
# Deliberately cheap: the staleness test is a handful of bash -nt comparisons,
# no directory walk. A directory's mtime changes when a file is added, removed,
# or renamed -- which is exactly when the database needs rebuilding. Editing an
# existing file does NOT change the directory mtime, and also does not need a
# rebuild, so the cheap signal is the correct one.
#
# cpp/Makefile is tested separately as a file, since editing it in place leaves
# the directory mtime untouched but can change the compile flags themselves.
#
# Does nothing if the database has never been generated: it is optional dev
# tooling, so we maintain it rather than create it uninvited. Run
# tools/gen_compile_commands.sh once to opt in.
# ---------------------------------------------------------------------------
refresh_compile_commands() {
    # Resolve an absolute root here rather than relying on PROJECT_ROOT, which
    # is relative and only correct while cwd is the project root.
    local root
    root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
    local db="$root/compile_commands.json"
    [ -f "$db" ] || return 0

    local stale=""
    if [ "$root/cpp/Makefile" -nt "$db" ]; then stale=1; fi
    if [ -z "$stale" ]; then
        for d in cpp cpp/core cpp/editline generated; do
            [ -d "$root/$d" ] || continue
            if [ "$root/$d" -nt "$db" ]; then stale=1; break; fi
        done
    fi
    [ -n "$stale" ] || return 0

    echo "Source tree changed; refreshing compile_commands.json..."
    # Never let a tooling hiccup fail the build.
    if ! "$root/tools/gen_compile_commands.sh" > /dev/null 2>&1; then
        echo "Warning: could not refresh compile_commands.json (clangd may be stale)."
    fi
    return 0
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
        dotnet build "$CSPROJ" -o build/cs/
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
        refresh_compile_commands
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

        refresh_compile_commands

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
        dotnet clean "$CSPROJ"
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
            echo "Platforms: mac-x86  mac-arm  mac  win  linux  csharp  all  package"
            exit 1
        fi

        if [ "$DIST_PLATFORM" = "mac" ]; then
            echo "Building Mac fat (universal) binary..."
            dist_build mac-x86 "$DIST_GOTO"
            dist_build mac-arm "$DIST_GOTO"
            lipo -create -output build/dist/miniscript-mac \
                build/dist/miniscript-mac-x86 \
                build/dist/miniscript-mac-arm
            echo "  Fat binary: build/dist/miniscript-mac"
        elif [ "$DIST_PLATFORM" = "all" ]; then
            echo "Building all distribution targets..."
            dist_build mac-x86 "$DIST_GOTO"
            dist_build mac-arm "$DIST_GOTO"
            lipo -create -output build/dist/miniscript-mac \
                build/dist/miniscript-mac-x86 \
                build/dist/miniscript-mac-arm
            echo "  Fat binary: build/dist/miniscript-mac"
            dist_build win   "$DIST_GOTO"
            dist_build linux "$DIST_GOTO"
            dist_csharp
            echo "All dist builds complete."
        elif [ "$DIST_PLATFORM" = "csharp" ]; then
            dist_csharp
        elif [ "$DIST_PLATFORM" != "package" ]; then
            dist_build "$DIST_PLATFORM" "$DIST_GOTO"
        fi

        # Bundle the results (with lib and docs) into the release archives.
        # "package" alone rebuilds every archive from the existing binaries,
        # e.g. after a doc or lib change.
        case "$DIST_PLATFORM" in
            all|package)          for p in mac win linux csharp; do dist_package "$p"; done ;;
            mac|win|linux|csharp) dist_package "$DIST_PLATFORM" ;;
        esac

        # Only a full 'dist all' can certify the whole set of artifacts that
        # push_release uploads.  A partial build leaves the other three
        # untouched and possibly built from older source, so rather than
        # vouch for them, drop the stamp: push_release then falls back to
        # comparing timestamps and says it has nothing to check against.
        # (Packaging alone leaves the binaries, and so the stamp, as they were.)
        if [ "$DIST_PLATFORM" = "all" ]; then
            record_build_stamp
        elif [ "$DIST_PLATFORM" != "package" ] && [ -f build/dist/.build-info ]; then
            rm -f build/dist/.build-info
            echo "  Partial dist build; removed build/dist/.build-info."
        fi
        ;;

    "test")
        # Unit tests plus the tests/testSuite.txt integration suite, on each
        # built executable.  --test always exits 0, so judge it by its output.
        echo "Running unit and integration tests..."
        suite_status=0
        for exe in build/cs/miniscript build/cpp/miniscript; do
            if [ ! -x "$exe" ]; then
                echo "  $exe: not built; skipped"
                continue
            fi
            echo "  $exe:"
            suite_out=$("$exe" --test 2>&1) || true
            if echo "$suite_out" | grep -q "^Unit tests complete\." \
                && echo "$suite_out" | grep -q "^Integration tests complete\." \
                && ! echo "$suite_out" | grep -q "^Some integration tests failed\."; then
                echo "$suite_out" | grep "^Integration tests:" | sed 's/^/    /'
            else
                echo "$suite_out" | sed 's/^/    /'
                echo "    FAILED"
                suite_status=1
            fi
        done

        # Regression: `input` at end of file returns null on both ports, rather
        # than crashing the host (C#) or returning "" (C++).  See notes/bugs.md
        # entry 2; this needs a closed stdin, so it can't live in testSuite.txt.
        echo "Testing input at end-of-file:"
        eof_expected=$'isNull: 1\nisString: 0'
        eof_status=0
        for exe in build/cs/miniscript build/cpp/miniscript; do
            [ -x "$exe" ] || continue
            eof_actual=$("$exe" tests/eof_input.ms < /dev/null 2>&1 | tail -2)
            if [ "$eof_actual" = "$eof_expected" ]; then
                echo "  $exe: OK"
            else
                echo "  $exe: FAILED (got: $eof_actual)"
                eof_status=1
            fi
        done
        [ $suite_status -eq 0 ] && [ $eof_status -eq 0 ] || exit 1
        ;;

    "test-all")
        echo "Running all test suites..."
        make -C tests all
        ;;

    "test-cs")
        echo "Running C# test suites..."
        make -C tests cs
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
        echo "Xcode project generated at cpp/xcode/miniscript.xcodeproj"
        echo "Open with: open cpp/xcode/miniscript.xcodeproj"
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
        echo "  test        - Run --test (unit + testSuite.txt integration tests)"
        echo "                and the EOF-input check on each built executable"
        echo "  test-all    - Run the C# unit tests under tests/"
        echo "  test-cs     - Same thing (tests/ holds only C# tests now)"
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
        echo "  $0 dist package          # Re-bundle archives (lib, docs) from existing binaries"
        echo ""
        echo "Mac signing and notarization (optional) are configured by tools/signing.secret;"
        echo "see the comment above load_signing_config in this script for its format."
        exit 1
        ;;
esac

echo "Build script completed successfully."
