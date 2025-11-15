# GPPG Parser Generator Tools

This directory contains the GPPG (Gardens Point Parser Generator) and GPLEX (Gardens Point LEX) tools used to generate the MiniScript parser from grammar files.

## What's Included

- **Gppg.dll** - LALR(1) parser generator (v1.5.3.1)
- **Gplex.dll** - Lexical scanner generator (v1.2.3.1)
- **Runtime config files** - Required for .NET 6+ execution

## Usage

These tools are invoked automatically by the build script:

```bash
./tools/build.sh parser
```

This command:
1. Reads grammar files from `grammar/miniscript.lex` and `grammar/miniscript.y`
2. Generates C# parser code in `cs/Parser/MiniScriptParser.cs` and `MiniScriptLexer.cs`

## Manual Usage

If you need to run the tools manually:

```bash
# Generate lexer
dotnet tools/gppg/Gplex.dll /out:output.cs input.lex

# Generate parser
dotnet tools/gppg/Gppg.dll /out:output.cs input.y
```

## Why Committed to Git?

These tool binaries are committed to the repository (rather than using NuGet or submodules) because:
- Ensures reproducible builds with specific versions
- Makes the project self-contained (only requires .NET runtime)
- Small size (~400KB total)
- No external dependencies during build

## License

GPPG and GPLEX are developed by Wayne Kelly and John Gough at QUT (Queensland University of Technology).
See the original copyright notices in the generated code.

## Documentation

- GPPG Documentation: https://github.com/k-john-gough/gppg
- GPLEX Documentation: Included in the GPPG repository
