# MiniScript 2.0 Test Suite

This directory contains all tests for the MiniScript 2.0 project, organized separately from production code.

## Directory Structure

```
tests/
├── cpp/                    # C++ tests
│   ├── unit/              # Unit tests for core/compiler components
│   ├── parser/            # Parser and lexer tests
│   ├── vm/                # Virtual machine tests
│   └── integration/       # End-to-end integration tests
├── cs/                    # C# tests
├── fixtures/              # Shared test data
│   ├── expressions/       # Expression test cases
│   ├── scripts/           # Full script test cases
│   └── expected_outputs/  # Expected output files
└── README.md             # This file
```

## Running Tests

### Quick Commands

From the project root:

```bash
# Run all tests
./tools/build.sh test-all

# Run C++ tests only
./tools/build.sh test-cpp

# Run C# tests only
./tools/build.sh test-cs

# Run specific test categories
./tools/build.sh test-parser
./tools/build.sh test-vm
```

### Using Make Directly

From the `tests/` directory:

```bash
# Run all tests
make all

# Run specific categories
make parser
make vm
make unit
make integration

# Clean test builds
make clean

# Show help
make help
```

### Running Individual Tests

```bash
# C++ Parser tests
cd tests/cpp/parser
make test

# C# Parser tests
cd tests/cs/parser
make test

# VM tests (when implemented)
cd tests/cpp/vm
make test
```

## Test Organization

### C++ Tests (`tests/cpp/`)

- **unit/** - Unit tests for individual components
  - Memory pool tests
  - String class tests
  - Dictionary tests
  - etc.

- **parser/** - Parser and lexer tests
  - Lexer tokenization tests
  - Parser grammar tests
  - AST generation tests

- **vm/** - Virtual machine tests
  - Opcode execution tests
  - Function call tests
  - Expression evaluation tests

- **integration/** - End-to-end tests
  - Full compilation pipeline
  - Cross-language consistency tests

### C# Tests (`tests/cs/`)

- **parser/** - Parser and lexer tests
  - Tests GPPG-generated parser with hand-written lexer
  - Same test cases as C++ parser tests
  - Ensures C# and C++ parsers produce identical results

### Test Fixtures (`tests/fixtures/`)

- Shared test input files
- Expected output files
- Test scripts and expressions

## Adding New Tests

### C++ Test Template

1. Create test file in appropriate directory:
```cpp
// tests/cpp/unit/test_my_feature.cpp
#include "path/to/my_feature.h"
#include <iostream>

int main() {
    // Test code here
    std::cout << "Test: my_feature" << std::endl;

    // Add assertions

    return 0;  // 0 = success
}
```

2. Add to Makefile in that directory

3. Run with `make test`

### C# Test Template

(To be added when C# test infrastructure is set up)

## Test Build Outputs

All test executables are built to:
```
build/tests/cpp/[category]/test_name
build/tests/cs/[category]/test_name
```

This keeps test builds separate from production builds.

## Prerequisites

Before running tests:

1. Build the core library:
```bash
cd cpp
make
```

2. Build the compiler library (for parser tests):
```bash
cd cpp/compiler
make
```

3. Then run tests:
```bash
./tools/build.sh test-all
```

## CI/CD Integration

(To be added: instructions for running tests in CI/CD pipelines)

## Debugging Tests

To debug a specific test:

```bash
# Build the test with debug symbols
cd tests/cpp/parser
make clean
CXXFLAGS="-g -O0" make

# Run with debugger
lldb build/tests/cpp/parser/test_parser
```

## Test Coverage

(To be added: instructions for generating coverage reports)

## Contributing

When adding new features:

1. Add tests in the appropriate `tests/` subdirectory
2. Ensure tests pass for both C# and C++ implementations
3. Update this README if adding new test categories
