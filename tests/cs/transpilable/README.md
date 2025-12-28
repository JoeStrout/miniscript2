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
├── layer0/                # Foundation (no dependencies)
│   ├── IOHelperTest.cs
│   ├── BytecodeTest.cs
│   ├── TestRunner.cs
│   └── Makefile
├── layer1/                # Basic Infrastructure
│   ├── ValueTest.cs       # CRITICAL - run first
│   ├── StringUtilsTest.cs # TODO
│   ├── MemPoolShimTest.cs # TODO
│   ├── TestRunner.cs
│   └── Makefile
├── layer2/                # Core Data Structures
│   ├── VarMapTest.cs      # TODO
│   ├── FuncDefTest.cs
│   └── Makefile
├── layer3/                # Processing Layer
│   ├── DisassemblerTest.cs # TODO
│   ├── AssemblerTest.cs    # TODO
│   └── Makefile
├── layer4/                # Execution Engine
│   ├── VMTest.cs          # TODO
│   └── Makefile
└── layer5/                # Top-level Components
    ├── VMVisTest.cs       # TODO
    └── Makefile
```

## Running Tests

### Quick Commands

From this directory:

```bash
# Run all tests in dependency order (stops on first failure)
make all

# Run specific layer
make layer0
make layer1
make layer2

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

Tests run in strict dependency order:
1. **Layer 0** (Foundation) - IOHelper, Bytecode
2. **Layer 1** (Basic Infrastructure) - Value, StringUtils, etc.
3. **Layer 2** (Core Data Structures) - VarMap, FuncDef
4. **Layer 3** (Processing) - Disassembler, Assembler
5. **Layer 4** (Execution) - VM
6. **Layer 5** (Top-level) - VMVis

**The test suite stops at the first layer that fails**, preventing cascading errors.

## Writing Tests

### Test File Template

Each test file should follow this pattern:

```csharp
// ModuleNameTest.cs
// Tests for ModuleName module (Layer N)

using System;
using static MiniScript.ValueHelpers;
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

1. **Write tests in C#** - Create test files in appropriate layer
2. **Test in C#** - Run `make all` to verify tests pass
3. **Transpile** - Use transpiler to generate C++ versions
4. **Test in C++** - Run transpiled tests in C++
5. **Compare results** - Both should produce identical output

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
- ✓ Layer 1: ValueTest (comprehensive)
- ✓ Layer 2: FuncDefTest (basic)

### TODO

- Layer 1: StringUtilsTest, MemPoolShimTest, ValueFuncRefTest
- Layer 2: VarMapTest
- Layer 3: DisassemblerTest, AssemblerTest
- Layer 4: VMTest
- Layer 5: VMVisTest

## Notes

- Tests are designed to be **identical** in C# and C++
- All output goes through `IOHelper.Print` for consistency
- No use of C#-specific or C++-specific features
- Tests verify both positive cases and error conditions
- Each layer can only test modules in that layer or below
