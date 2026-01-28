## C# Coding Standards

To make our transpiler work, we must be very disciplined in how we write C# code.  This means both limiting what C# features we use, and standardize the way in which we write it.  Here we attempt to document all the restrictions.

## Data Types (Basic)

Use the type names from `System` instead of the shorthand types.  For example, instead of `int`, write `Int32`.  (These compile to the same thing in C# but are more explicit.)

Including non-integer types, the full set of types you can use are: `Byte`, `SByte`, `Int16`, `UInt16`, `Int32`, `UInt32`, `Int64`, `UInt64`, `Char`, `Single`, `Double`, `Boolean`, `String`, `List<>`.

## Data Types (Class/Struct/Etc.)

Consider carefully what you use to encapsulate functions and data.  Rules of thumb:

- Use `struct` for anything small enough to be passed around on stack and safely copied.  These become `struct` values (stack-based, copied) in C++.
- Use `class` for larger chunks of data, or when you need reference semantics.  These become a pair of classes in C++: a storage class and a `shared_ptr` wrapper.  These use reference-counting, so avoid reference cycles, or be careful to break them!
- Use `static class` when you need no data at all, but just need to collect together a bunch of static methods or constants.
- The only generic classes allowed currently are `List` and `Dictionary`.

Summary of how various C# concepts transpile to C++ (or not):

- class  →  class (with our smart_ptr wrapper approach)
- struct  →  struct
- enum  →  enum class
- interface  →  struct (with only abstract methods)
- delegate: disallowed (or handled as a special case)
- partial: disallowed
- record: disallowed
- custom generics: disallowed

## Very limited use of properties

A C# property has code for a getter and/or setter.  The closest C++ equivalent would be an inline method or pair of methods.  This makes transpilation awkward.  So, the C# could should use fields rather than properties wherever possible, and especially avoid properties with setters.

## String Interpolation

Our transpiler converts C# string interpolation to a call to our `Interp` method (in CSString.h/.cpp): C# `$"{x}"` → C++ `Interp("{}", x)`
- Supports format specifiers: `{0.00}` for decimal places
- Uses variadic templates (C++14 compatible)
- Type conversions: int, double, const char*, String


## Indentation

The transpiler may rely on indentation to disambiguate code where it would otherwise take a full C# parser to do so.  So, it is important to follow these indentation rules:

1. **Indent with one tab per level.**  No indenting with spaces.  Ever.
2. **Don't indent for the `namespace` block.**  Currently we target C# 9, so we can't use the file-scoped namespace feature of C# 10, so we follow the (common) style of not indenting within the `namespace Foo { ... }` block.

The result of these rules is that a class, struct, or enum declaration will always be flush-left (not indented), and their members will be indented with exactly one tab.

Example:
```csharp
namespace MyNamespace {

public class MyClass {
	public int value;

	public void DoSomething() {
		value = 42;
	}
}

}
```

## Brace placement

- Always put an **opening brace at the end of a line**, never on a line by itself.

- Put a **close brace at the start of a line**.  It will usually be the *only* thing on that line, except for something like `} else {` or in a `switch` statement, `} break;`.


## No indented blocks without braces

Never, _ever_ do something like this:

```
        if (Value.Equal(_items[i], item)) 
            return i;
```

Nor this:

```
        for (int i = 0; i < s.Length; i++)
            parts[i] = s[i].ToString();
```

This is evil and dangerous in any C-derived language.  Either put it all on one line, or use proper curly braces.  Do this:

```
        if (Value.Equal(_items[i], item)) return i;
```

Or do this:

```
        for (int i = 0; i < s.Length; i++) {
            parts[i] = s[i].ToString();
        }
```

## Inline Methods

To make a method inline, add these `using` clauses at the top of your .cs file:
```
using System.Runtime.CompilerServices;
using static System.Runtime.CompilerServices.MethodImplOptions;
```
...and then decorate your method with `[MethodImpl(AggressiveInlining)]`.

Example:
```
using System.Runtime.CompilerServices;
using static System.Runtime.CompilerServices.MethodImplOptions;

public struct Token {
	public TokenType type;
	public String text;

	[MethodImpl(AggressiveInlining)]
	public Token(TokenType type, String text = null) {
		this.type = type;
		this.text = text;
	}
}
```

This will cue the transpiler to put the method body in the header, rather than in the .cpp file.

## Memory Management Considerations

### `class` vs. `struct`

Use `struct` for simple classes containing a relatively small amount of data.  In C++, these will be passed around as structs on the stack.  They use copy semantics in both C# and C++.

Use `class` for things that contain more data, or where you really need reference semantics.  In C++, such a class becomes a pair of classes: a reference-counted storage class and a thin wrapper class.

### Class Constructors

C# lets you instantiate a class that does not have an explicit constructor.  The transpiler converts `new Foo` instantiation to `Foo::New()`, but it only makes this `New` factory method when it sees an explicit constructor.  So, every class you might `new` needs to have an explicit constructor, even if it is empty.



## Other limitations

- The `switch` statement may only be used with integer types (including enums).  For Strings or other custom types, use `if` statements instead.

## Capitalization

While not strictly required for the transpiler, in this project we follow C# capitalization conventions:

- All class names use PascalCase
- All public members (fields, methods, properties) also use PascalCase
- Private/protected fields use _underscoreCamelCase
- Parameters and local variables use camelCase

Example:
```
public class MySuperClass {
    public Int32 MyIntField;
    private String _secretStuff;
    
    public void DoThings(Int32 withValue, Boolean quickly) {
    	Int64 temp = withValue * 7;
    	if (!quickly) Utils::DoSlowThing();
    	DoQuickThing(temp);
    }
}
```


    
## To be continued...

We'll add more restrictions here as we remember/implement them.
