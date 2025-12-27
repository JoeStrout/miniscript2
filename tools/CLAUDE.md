# Transpiler2 - C# to C++ Transpiler

## Purpose

This project is a C# to C++ transpiler written in MiniScript. The transpiler targets C++14 and is designed to be compatible with Unity (which currently supports C# 9).

## Target C# Feature Set

- **C# Version**: C# 9 (Unity compatibility requirement)
- **Key Features Supported**:
  - Static classes with const fields
  - Structs with constructors and methods
  - Enums (translated to `enum class`)
  - Classes with inheritance and polymorphism
  - String interpolation (`$"{value}"` → `Interp("{}", value)`)
  - Method inlining via `[MethodImpl(AggressiveInlining)]`

## C# Coding Conventions (see CS_CODING_RULES.md)

1. **Indentation**: One tab per level, NO spaces
2. **Namespace blocks**: Not indented (common C# 9 style, since file-scoped namespaces aren't available until C# 10)
3. **Brace placement**: End of line (K&R style)
4. **Result**: Top-level declarations (class/struct/enum) are always flush-left

## Translation Approach

### Static Classes
C# static classes → C++ regular classes with static members
```csharp
public static class Op {
    public const String PLUS = "PLUS";
}
```
→
```cpp
class Op {
    public: static const String PLUS;
};
const String Op::PLUS = "PLUS";
```

### Structs
Straightforward translation with inline constructors in header
```csharp
public struct Token {
    public TokenType type;
    public Token(TokenType type) { this.type = type; }
}
```
→
```cpp
struct Token {
    public: TokenType type;
    public: inline Token(TokenType type);
};
inline Token::Token(TokenType type) { this->type = type; }
```

### Classes (Wrapper Pattern)

**This is the complex one!** C# classes are translated using a shared_ptr wrapper design pattern to maintain value semantics while supporting polymorphism:

Each C# class becomes TWO C++ classes:
1. **Storage class** (`ClassNameStorage`) - contains actual data, lives on heap, managed by shared_ptr
2. **Wrapper struct** (`ClassName`) - lightweight, contains just a shared_ptr, passed by value

**Example Translation:**
```csharp
public abstract class ASTNode {
    public abstract String Str();
}

public class NumberNode : ASTNode {
    public Double value;
    public NumberNode(Double value) { this.value = value; }
    public override String Str() { return $"{value}"; }
}
```

→

```cpp
// 1. Base wrapper (forward declarations first)
struct ASTNode {
    protected: std::shared_ptr<ASTNodeStorage> storage;
    public: ASTNode(std::shared_ptr<ASTNodeStorage> stor) : storage(stor) {}
    public: ASTNode() : storage(nullptr) {}
    public: String Str();  // Calls through to storage
};

// 2. Storage classes (contain actual implementation)
class ASTNodeStorage : public std::enable_shared_from_this<ASTNodeStorage> {
    public: virtual String Str() = 0;
};

class NumberNodeStorage : public ASTNodeStorage {
    public: Double value;
    public: NumberNodeStorage(Double value);
    public: String Str();
};

// 3. Base wrapper method implementations
inline String ASTNode::Str() { return storage->Str(); }

// 4. Derived wrapper
struct NumberNode : public ASTNode {
    public: NumberNode(Double value)
        : ASTNode(std::make_shared<NumberNodeStorage>(value)) {}
    private: NumberNodeStorage* get() {
        return static_cast<NumberNodeStorage*>(storage.get());
    }
    public: Double get_value() { return get()->value; }
    public: void set_value(Double _v) { get()->value = _v; }
};

// 5. Inline storage methods (for [MethodImpl(AggressiveInlining)])
inline NumberNodeStorage::NumberNodeStorage(Double value) {
    this->value = value;
}
inline String NumberNodeStorage::Str() {
    return Interp("{}", value);
}
```

**File Organization:**
- `.g.h` files contain:
  1. Forward declarations
  2. Base wrapper class
  3. All storage classes (declarations)
  4. Base wrapper method implementations (inline)
  5. Derived wrapper classes
  6. Inline method implementations (methods marked `[MethodImpl(AggressiveInlining)]`)
- `.g.cpp` files contain:
  - Non-inline method implementations (regular methods without inline attribute)

**Key Helper Functions:**
- `IsNull(ASTNode node)` - checks if wrapper contains null storage
- `As<WrapperType, StorageType>(ASTNode node)` - type-safe casting (C#'s `as` operator)

## C++ Support Infrastructure

Located in `../../cpp/core/` (main repo's core directory):

### Core Types (core_includes.h)
- C# type aliases: `Int32`, `Double`, `Boolean`, etc.
- Collection classes: `String`, `List<T>`, `Dictionary<K,V>`

### String Class (CS_String.h/cpp)
- Wrapper around `StringStorage` (C implementation)
- Uses `shared_ptr` with custom deleter for memory management
- **Important**: `wrapStorage()` helper prevents double-free when StringStorage functions return input pointer
- Implicit conversion to `const char*` for compatibility with `std::cout` and C APIs

### String Interpolation (CS_String.cpp)
`Interp(format, args...)` - C# `$"{x}"` → C++ `Interp("{}", x)`
- Supports format specifiers: `{0.00}` for decimal places
- Uses variadic templates (C++14 compatible)
- Type conversions: int, double, const char*, String

## Test Infrastructure

### Directory Structure
```
tools/
├── transpile.ms        # The transpiler (MiniScript)
└── tests/
    ├── staticStringConst/  # Test: static class with const strings
    ├── struct/             # Test: struct with enum
    ├── class/              # Test: class hierarchy with polymorphism
    └── Makefile            # Build system

../../cpp/core/         # C++ support library (String, List, etc.)
```

### Each Test Contains
- `ClassName.cs` - C# source to transpile
- `main.cs` - C# test driver
- `main.cpp` - C++ test driver
- `expected/ClassName.g.h` - Expected C++ header output
- `expected/ClassName.g.cpp` - Expected C++ source output
- `generated/` - (future) Transpiler output goes here

### Running Tests

```bash
cd tests

# Run all C# tests
make test-all-cs

# Run all C++ tests (expected output)
make test-all-expected

# Run all C++ tests (generated output - once transpiler works)
make test-all-generated

# Run specific test
make test TEST=struct VARIANT=cs      # C# version
make test TEST=struct VARIANT=exp     # C++ expected
make test TEST=class VARIANT=gen      # C++ generated
```

### Build System Details
- Compiles C++ code with: `-std=c++14 -Wall -Wextra -Wno-switch`
- `-Wno-switch` needed because C# doesn't require exhaustive enum switches
- Links with C support library (StringStorage.c, unicodeUtil.c, hashing.c)
- Uses both C and C++ compilers (gcc for .c, g++ for .cpp)

## Implementation Notes

### String Interpolation Translation
C#: `$"Value: {x}"`
→ C++: `Interp("Value: {}", x)`

C#: `$"Pi: {pi:0.00}"`
→ C++: `Interp("Pi: {0.00}", pi)`

### Scope Declarations
C++ output includes explicit `public:`, `private:`, `protected:` on each member to mirror C# code structure (non-standard but aids readability).

### Method Inlining
- C# methods marked with `[MethodImpl(AggressiveInlining)]` → `inline` in C++
- Inline methods in storage classes go in the .h file (section 5)
- Regular methods go in the .cpp file

### Null Handling
- C# `null` → C++ `nullptr` for pointers/storage
- Empty strings represented as `nullptr` in StringStorage
- Wrapper classes check for null storage before dereferencing

## Next Steps

The transpiler itself (MiniScript code) needs to be written to:
1. Parse C# code (or use simplified input format)
2. Identify class type (static class, struct, or reference class)
3. Generate appropriate C++ wrapper/storage pattern
4. Handle method inlining attributes
5. Translate string interpolation to Interp() calls
6. Output .g.h and .g.cpp files

Test-driven development: verify output matches `expected/` files for each test case.
