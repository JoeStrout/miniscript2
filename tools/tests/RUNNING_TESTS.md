## Run all tests:

- `make test-all-cs` - Run all C# tests
- `make test-all-expected` - Run all C++ tests with expected output
- `make test-all-generated` - Run all C++ tests with transpiler-generated output

## Run specific test:

- `make test TEST=struct VARIANT=cs` - Run struct test in C#
- `make test TEST=class VARIANT=cpp-expected` - Run class test with expected C++
- `make test TEST=struct VARIANT=exp` - Run expected C++ version (synonym for cpp-expected)
- `make test TEST=struct VARIANT=gen` - Run generated C++ version (synonym for cpp-generated)

## Cleanup:

- `make clean` - Remove all build artifacts
