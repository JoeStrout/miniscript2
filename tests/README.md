# MiniScript 2.0 Test Suite

## Where the tests actually are

**Most of the coverage lives in the interpreter binary, not in this directory:**

```bash
build/cpp/miniscript2 --test     # or build/cs/miniscript2 --test
```

That runs `UnitTests.RunAll` (`cs/UnitTests.cs`) followed by the integration
suite in `tests/testSuite.txt` -- ~800 cases of MiniScript source paired with
its expected output.  Adding a case there is usually the right way to cover a
language- or VM-level change.

This directory additionally holds a small layered C# unit-test tree
(`cs/transpilable/`), written so it can be transpiled to C++ the same way the
interpreter is.  Only layer 0 remains: layers 1-4, and the parallel C++ tree
under `tests/cpp/`, were retired in 2026 after bit-rotting past the point of
repair -- they were written against a `Value` API of free functions
(`make_int`, `value_add`, `list_get`) that the C# side never had, against a
hand-written `cpp/compiler/` that no longer exists, and against core sources
(`cpp/core/value.c`, `gc.c`) that have since moved or gone.  Look for them in
git history rather than trusting anything that references them.

```
tests/
├── testSuite.txt          # the integration suite, run by --test
├── eof_input.ms           # fixture: `input` at end of file (build.sh test)
└── cs/
    └── transpilable/      # layered C# unit tests
        ├── TestFramework.cs
        └── layer0/        # IOHelper, Bytecode
```

## Running Tests

From the project root:

```bash
./tools/build.sh test        # quick smoke test of the built executables
./tools/build.sh test-all    # the C# unit tests under tests/
./tools/build.sh test-cs     # same thing; tests/ holds only C# tests now
```

Or with make, from `tests/`:

```bash
make all      # run everything here
make clean    # clean test build artifacts
make help
```

Layer 0 on its own:

```bash
cd tests/cs/transpilable/layer0 && make test
```

## Adding a Test

**A language, VM, or intrinsic behavior:** add a case to `testSuite.txt`.  The
format is a block of MiniScript source, then `=====`, then the exact expected
output.

**A C# unit test of a low-level module:** add it under
`cs/transpilable/layer0/`, register it in that layer's `TestRunner.cs`, and add
the file to the layer `Makefile`'s `SOURCES`.  Keep the layering honest -- a
layer 0 test may only depend on modules with no dependencies of their own.
Assert on behavior, not on incidental numbering: the old layer 0 test pinned
`RETURN` to opcode 61 and simply broke when an opcode was inserted ahead of it.
