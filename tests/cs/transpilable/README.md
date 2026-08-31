# Transpilable C# Tests

This directory contains C# tests that are designed to be transpiled to C++ and run identically in both environments. This allows us to:

1. Test the C# implementation
2. Validate the transpiler itself
3. Test the transpiled C++ code
4. Ensure consistency between C# and C++ implementations

## Directory Structure

Tests are organized by dependency layers:

```
transpilable/
├── TestFramework.cs       # Transpilable assertion framework
└── layer0/                # Foundation (no dependencies)
    ├── IOHelperTest.cs
    ├── BytecodeTest.cs
    ├── TestRunner.cs
    └── Makefile
```

Layers 1-4 existed once and were retired in 2026; see "History" at the bottom
before recreating them.

## Running Tests

### Quick Commands

From this directory:

```bash
# Run all tests in dependency order (stops on first failure)
make all

# Run specific layer
make layer0

# Clean all test builds
make clean
```

From project root:

```bash
# Run all transpilable C# tests
cd tests/cs/transpilable
make all
```

### Test Execution Order

Tests run in strict dependency order, and **the suite stops at the first layer
that fails**, preventing cascading errors.  Only layer 0 (IOHelper, Bytecode)
is populated today.  A new layer N may depend only on modules in layers below
it -- a rule worth checking against reality before adding one, since `Value`,
the obvious layer 1 candidate, now reaches into `FuncDef`, `VM` and the GC.

## Writing Tests

### Test File Template

Each test file should follow this pattern:

```csharp
// ModuleNameTest.cs
// Tests for ModuleName module (Layer N)

using System;
// CPP: #include "ModuleName.g.h"
// CPP: #include "IOHelper.g.h"
// CPP: #include "TestFramework.g.h"

namespace MiniScript {

public static class ModuleNameTest {

    public static Boolean TestSomething() {
        // Setup
        // ...

        // Test
        Boolean ok = true;
        ok = TestFramework.Assert(condition, "description") && ok;
        ok = TestFramework.AssertEqual(actual, expected, "context") && ok;

        return ok;
    }

    // Main test runner for this module
    public static Boolean RunAll() {
        IOHelper.Print("");
        IOHelper.Print("=== ModuleName Tests ===");
        TestFramework.Reset();

        Boolean allPassed = true;
        allPassed = TestSomething() && allPassed;
        // ... more tests

        TestFramework.PrintSummary("ModuleName");
        return TestFramework.AllPassed();
    }
}

}
```

### Key Requirements

1. **Static test methods** - All test methods must be static
2. **Return Boolean** - Test methods return true if all assertions pass
3. **Use TestFramework** - Use TestFramework.Assert* methods for all assertions
4. **CPP comments** - Include `// CPP:` directives for transpiler
5. **RunAll() method** - Each test class must have a RunAll() method
6. **No external dependencies** - Tests should be self-contained within their layer

### TestFramework API

Available assertion methods:

```csharp
// Boolean assertion
TestFramework.Assert(condition, "message")

// Equality assertions (multiple overloads)
TestFramework.AssertEqual(actual, expected, "context")
TestFramework.AssertEqual(intActual, intExpected, "context")
TestFramework.AssertEqual(strActual, strExpected, "context")
TestFramework.AssertEqual(boolActual, boolExpected, "context")

// Test suite management
TestFramework.Reset()              // Reset counters
TestFramework.PrintSummary("name") // Print results
TestFramework.AllPassed()          // Returns true if no failures
```

## Transpilation Workflow

### Running C# Tests

From this directory:

```bash
# Run all C# tests in dependency order
make all

# Run specific layer
make layer0
```

### Transpiling and Running C++ Tests

There is no C++ side any more.  `tests/cpp/` held transpiled copies of these
tests plus stub `vm`/`unit`/`integration` targets; it was retired in 2026 (see
"History").  Recreating it means writing a layer Makefile against the current
`cpp/core` layout -- the old ones still named `value.c` and `gc.c`.

## Adding New Tests

When adding tests for a new module:

1. **Identify the layer** - Determine which dependency layer it belongs to
2. **Create test file** - `[ModuleName]Test.cs` in the appropriate layer directory
3. **Write tests** - Follow the template above
4. **Update TestRunner** - Add your test to the layer's TestRunner.cs
5. **Update Makefile** - Add source files to the layer's Makefile
6. **Verify** - Run `make all` to ensure tests work

## Test Coverage Goals

Each module should have tests for:

- **Creation/initialization** - Verify objects can be created
- **Basic operations** - Test core functionality
- **Edge cases** - Empty inputs, null values, boundary conditions
- **Error handling** - Invalid inputs, error conditions
- **Integration** - Interaction with other modules (within layer limits)

## Current Status

### Implemented

- ✓ TestFramework.cs - Core assertion infrastructure
- ✓ Layer 0: IOHelperTest, BytecodeTest

### History

Layers 1-4 (ValueTest, StringUtilsTest, FuncDefTest, Assembler/Disassembler
tests, VMTest) and the whole `tests/cpp/` tree were removed in 2026.  They had
stopped building long before: `ValueTest.cs` was written against a free-function
`Value` API (`make_int`, `value_add`, `list_get`, `map_set`) that exists only in
the C++ core and never in C#; the layer Makefiles still listed `cs/MemPoolShim.cs`,
deleted in January 2026; and the C++ Makefiles referenced `cpp/compiler/` and
`cpp/core/value.c`, both since gone.  They are in git history if you want them.

The coverage they were meant to provide is now carried by `miniscript2 --test`
(unit tests plus ~800 integration cases in `tests/testSuite.txt`).  Bring a
layer back only if there is something that suite genuinely cannot reach.

## Notes

- Tests are designed to be **identical** in C# and C++
- All output goes through `IOHelper.Print` for consistency
- No use of C#-specific or C++-specific features
- Tests verify both positive cases and error conditions
- Each layer can only test modules in that layer or below
