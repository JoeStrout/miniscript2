Title: **Zig-based build system**
Status: Accepted

## Context

We need an easy way to build command-line MiniScript for Mac (both Intel and Apple Silicon), Windows, and Linux.  This should, as much as possible, produce clean self-contained binaries that don't require the user to install anything else (e.g. DLLs) to make it work.  And ideally, it should be possible for the primary maintainer (Joe) to build all three binaries from his (Mac) development machine, for rapid iteration especially during active development.  It might also be nice to have these built via GitHub actions, especially if cross-compiling is not possible.

## Decision

Build via [Zig C++](https://zig.guide/working-with-c/zig-cc]), a cross-platform wrapper for Clang with its own libc/libc++ implementation for many targets (including the ones we care about).

## Alternatives Considered

The main alternative is to give up cross-compilation and build on each platform (either manually or via GitHub Actions).  For Mac and Linux, standard toolchains work fine; the big question was how to handle Windows:

- MSVC via `cl.exe`: lose computed goto (slight performance hit), and we need a CMake or Makefile that works with MSVC's syntax differences.
- MinGW-w64 via MSYS2: gives us GCC, computed-goto, and our existing Makefile works with minimal changes.  But we would statically link the MinGW libs, which would increase the exe size by maybe 5ish MB.
- clang-cl: Clang with an MSVC-compatible driver; makes a nice clean binary, though does not support GCC extensions such as computed goto.

But honestly the tradeoffs above are minor compared to the convenience of cross-compiling that only Zig provides.

## Consequences

- Developers can trivially build all three (Mac/Win/Linux) executables via cross-compilation.  (Note that non-Mac users will need the macOS SDK to make the Mac build.)

- The Mac builds can be Intel (`x86_64`) or Apple Silicon (`aarch64`); and then we can use `lipo` to combine them into a universal binary.

- Computed-goto should work on at least Linux (with -target x86_64-linux-musl) and Mac; it's not clear whether the Windows version will support that (via windows-gnu), but we could fall back to `switch` if necessary.

- GitHub Actions can use the Zig system as well, if desired.


