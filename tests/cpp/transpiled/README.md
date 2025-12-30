# Transpiled C++ Tests

This directory contains C++ test code that has been transpiled from the C# tests in `tests/cs/transpilable/`. These tests validate:

1. The transpiler correctly converts C# to C++
2. The C++ runtime behaves identically to the C# implementation
3. Cross-platform consistency between C# and C++ code

## Directory Structure

```
transpiled/
├── TestFramework.g.cpp    # Transpiled test framework
├── TestFramework.g.h
├── layer0/                # Foundation tests
│   ├── main.cpp           # C++ test entry point
│   ├── IOHelperTest.g.cpp
│   ├── BytecodeTest.g.cpp
│   ├── TestRunner.g.cpp
│   └── Makefile
├── layer1/                # Basic Infrastructure tests
├── layer2/                # Core Data Structures tests
└── ...
```

## Running Tests

### Prerequisites

- C# tests must pass first (run from `tests/cs/transpilable/`)
- MiniScript transpiler must be available (`tools/transpile.ms`)
- C++ compiler (g++ or clang++)

### Running a Specific Layer

From a layer directory (e.g., `layer0/`):

```bash
# Transpile C# tests to C++
make transpile

# Build and run C++ tests
make test

# Or simply (transpile is separate, test includes build)
make all
```

### Running All Layers

Currently, each layer must be run manually:

```bash
# From tests/cpp/transpiled/
cd layer0 && make transpile && make test && cd ..
cd layer1 && make transpile && make test && cd ..
cd layer2 && make transpile && make test && cd ..
```

## Makefile Targets

Each layer's Makefile supports:

- `transpile` - Transpile C# test files to C++
- `build` - Compile C++ test executable
- `test` - Build and run tests (default)
- `all` - Same as `test`
- `clean` - Remove build artifacts and generated .g.cpp files

## Workflow

1. **Write/modify C# tests** in `tests/cs/transpilable/layerN/`
2. **Verify C# tests pass** with `make test` in C# layer directory
3. **Transpile to C++** with `make transpile` in corresponding C++ layer
4. **Run C++ tests** with `make test`
5. **Compare output** - Should be identical to C# output

## Expected Output

The C++ test output should exactly match the C# test output:

```
=== IOHelper Tests ===
PASS: IOHelper.Pad works
PASS: IOHelper.PadLeft works
...
IOHelper: 5 passed, 0 failed
```

Any discrepancies indicate either a transpiler bug or C++ runtime issue.

## Troubleshooting

### Transpilation Fails

- Ensure MiniScript is installed and `miniscript` is in PATH
- Check that `tools/transpile.ms` exists
- Verify C# source files exist in `tests/cs/transpilable/`

### Compilation Fails

- Check that required C++ core files exist in `cpp/core/`
- Verify production code is transpiled in `generated/`
- Review compiler errors for missing includes or undefined references

### Tests Produce Different Output

- This indicates a bug - either in the transpiler or C++ runtime
- Compare C# vs C++ output line-by-line
- Check for differences in numeric precision, string handling, or control flow

## Notes

- All `.g.cpp` and `.g.h` files are auto-generated - do not edit manually
- The `main.cpp` file in each layer is hand-written C++ entry point
- Tests use the same `TestFramework` that's transpiled from C#
- Production code dependencies come from `generated/` directory
