# MiniScript 2.0 — C# / .NET Build

This archive contains a framework-dependent .NET build of MiniScript 2.0,
intended as a fallback for platforms not covered by the native binaries.

## Files

- **miniscript2-csharp.dll** — the managed .NET assembly (the interpreter itself)
- **miniscript2-csharp.runtimeconfig.json** — runtime configuration required by the .NET host

## Requirements

.NET 6 or later must be installed. Download from https://dotnet.microsoft.com/download

## Usage

    dotnet miniscript2-csharp.dll [script.ms]

If no script file is given, the interpreter starts in interactive (REPL) mode.

## Why use this build?

The native binaries (miniscript2-mac, miniscript2-linux, miniscript2-win.exe)
are faster and self-contained. Use this C# build only if no native binary is
available for your platform (e.g. ARM Linux, FreeBSD, etc.).
