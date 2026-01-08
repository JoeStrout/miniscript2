## Dec 28 2025

I haven't been keeping a dev lop up to now, but it's high time that I did.  The main purpose of this is to externalize my memory of decisions made, to-do items, etc.

Current status:
- We have a C++ core (hand-written) that implements a Value type and related support code, as well as some C++ equivalents of common C# types (String, List, Dictionary).
- We have a bytecode format, VM, assembler, and disassembler, all written in C# and transpilable to equivalent C++ code.
- We have a minimal start on a MiniScript parser (numeric expressions only), generated using the Gardens Point Parser Generator (with grammar defined in .lex/.y files).

I've recently developed a couple of prototypes that now need to be rolled into the main project:

1. A Pratt parser for MiniScript numeric expressions.  (See MS2Proto5 in the MS2Prototypes project.)  This will replace the Gardens Point generated parser.
2. A rewritten transpiler that leans heavily into shared_ptr and thin wrappers for non-value types.  It also supports C# string interpolation.  (See MS2Prototypes/Transpiler2.)

So, getting these into the main project is the top priority, but the second one especially is likely to be somewhat intricate work.  I'm going to take it in stages, based on the dependency layers in the C# code:

Layer 0 (Foundation - no dependencies):
- IOHelper
- Bytecode
- ShiftReduceParserCode

Layer 1:
- Value (uses internal classes only)
- StringUtils (→ IOHelper, Value)
- MemPoolShim (→ IOHelper, StringUtils; likely no longer needed at all)
- ValueFuncRef (→ Value)
- Lexer (→ ShiftReduceParserCode)

Layer 2:
- VarMap (→ Value)
- FuncDef (→ Value, StringUtils)

Layer 3:
- Disassembler (→ Bytecode, FuncDef, StringUtils)
- Assembler (→ Value, Bytecode, FuncDef, StringUtils, IOHelper)

Layer 4:
- VM (→ Value, Bytecode, FuncDef, IOHelper, Disassembler, StringUtils, VarMap)

Layer 5:
- VMVis (→ Value, Bytecode, FuncDef, IOHelper, VM, StringUtils)
- UnitTests (→ Value, IOHelper, StringUtils, Disassembler, Assembler, Bytecode)

Layer 6 (Top - Application):
- App (→ UnitTests, VM, VMVis, IOHelper, Assembler, Value)

To support this work, I'll need better testing infrastructure.  The C# tests should also  be transpilable.  So I'm starting there today.

## Dec 30, 2025

Transpilable unit tests for layers 0 and 1 are complete, and turned up a couple of GC-related bugs in value_map.c that are now fixed.

But those layers didn't involve anything more complex than static classes.  Next up is layer 2, which involves FuncDef.  FuncDef is currently a class, but it has no subclasses or virtual methods.  So it doesn't need all the complexity our class transpilation assumes.  But I'm not sure it should be a struct, either; it has quite a bit of data to it.

It's too late tonight, but tomorrow, I'll need to really think about the memory management strategy for FuncDef, and possibly upgrade the transpiler to handle this case of a shared_ptr wrapper class without an abstract base class.


## Dec 31, 2025

The transpiler did indeed need a bit of work to get non-abstract base classes working.  That's mostly good now, though there are still issues figuring out when to change `.` into `::` for C++.

However, the FuncDefTest fails to compile because it depends on StringUtils, and it turns out that StringUtils did not transpile correctly.  That was supposed to be tested in level 1, but was overlooked.  So, it's time to step back to level 1 and add that.


## Jan 01, 2026

I'm still working on getting the new transpiler to do all the things the old one could do.  The rewrite caused quite a lot of regressions, as expected.  But, the new code structure is much cleaner, and is remaining so even as these additional features are added.  So it will all be worth it in the end.

The biggest problem I'm still struggling with is correctly telling when to change "." to "::".  The new transpiler builds a symbol table which is supposed to handle this, but it's not working in all cases.


## Jan 06, 2026

The problem with the transpiler was just that it was not tracking local variables properly; method parameters were not included, nor were local array declarations properly handled.  Those issues have been fixed, and the StringUtilsTest (part of our "layer 1" transpilable tests) is now passing all tests in both C# and C++.

Had some adventures with ambiguous overloads, mainly in our test framework where there is an AssertEqual method that takes Booleans.  Pointers -- including C string literals -- implicitly convert to bool in C++, and there's nothing you can do to stop that.  But we now have a Boolean type that is a wrapper for bool (not just an alias), which makes the conversion to Boolean take one more step, and with that we can avoid the ambiguity.  While we're at it, our Boolean wrapper also deletes the implicit conversion from pointer types to Boolean, which will help prevent accidental assignment or passing of a pointer to a Boolean argument.  (When we actually want to test a pointer, we'll need to use `!= nullptr`, similar to how you do it in C#.)

One of the recent changes to transpile.ms is that it needs to have pre-baked code about the core (non-transpiled) classes, so it can properly interpret symbols related to them.  I've put this in coreClassInfo.ms, but the classes defined there (String and List) are very incomplete; I've only coded in as much as I need so far.

That takes care of the C# standard library classes, but now I'm running into a similar issue trying to transpile FuncDefTest (layer2), in that it does not understand the FuncDef class.  And that class *is* transpiled; it's just not part of the context the transpiler has when working on this test.  Hmm.

I need to turn to other tasks for now, but next session, I need to make some way to give the compiler more context.  Perhaps a command-line argument for files to read in the first pass, but not actually transpile in the second... or maybe it *should* transpile them, but send the results to a different folder.  Or maybe transpiling should output symbol info (in JSON or GRFON) that a later transpilation can pick up.


## Jan 07, 2026

I've decided to add a `-c <context_path>` option to the transpiler; if given, it will scan (but not actually transpile) all the .cs files in that directory for context.

So, that works now and gets us a lot farther.  But we still have some issues with the layer2 FuncDef class:

1. Methods like ReserveRegister are being properly placed on the Storage class, but the call-through method in the wrapper class is missing.
2. Our type analysis is getting confused by the auto-generated getter methods, like Code() and Constants() on FuncDef, and so we're converting `.` to `::` when we should not.
3. The header-only `operator bool` code doesn't know where to do in a smart-ptr-wrapper class.  Right now it's just landing out in free space between the two classes.  :|

## Jan 08, 2026

OK, I fixed the issues above.  Issue 2 boiled down to simply not having the Add method declared in coreClassInfo.ms -- oops.  I've added all the List methods and properties we are likely to use.  I also cleaned up handling of comments before a method, and fixed inline method handling.  Stuff is looking pretty good as far as FuncDef (my current test case) goes.

But when I try to run the layer2 test it segfaults, and this is because our test tries to allocate a FuncDef in C#, by doing `FuncDef func = new FuncDef();`, which currently transpiles to just `FuncDef func = FuncDef();`.  This never actually allocates FuncDefStorage in C++.  My analysis:

The point of this Storage/wrapper pattern is to make the C++ semantics mirror the C# semantics as closely as possible.  In C#, I _can_ have a null FuncDef, which I would 
declare as:
`FuncDef func; // currently null`
In C++, as currently written, I can also do the same:
`FuncDef func; // currently a wrapper containing null storage`
But when C# code does `new FuncDef()`, that actually allocates the FuncDef data 
structure.  So the C++ code needs to do the same, i.e., call some factory method or a version of the constructor that allocates storage.
But note that if our data type is a `struct` (and so does _not_ use our wrapper pattern), then `MyStruct foo = MyStruct();` would suffice, doing exactly the same thing in both C# and C++.

Claude remarks: Your understanding is correct.

  For C# classes (reference types) → C++ wrapper pattern:
  - FuncDef func; in C# (null reference) → FuncDef func; in C++ (wrapper with null storage) ✓
  - new FuncDef() in C# (heap allocation) → needs FuncDef::Create() or equivalent in C++

  For C# structs (value types) → C++ plain struct, no wrapper:
  - MyStruct foo = new MyStruct(); in C# → MyStruct foo = MyStruct(); in C++ ✓
  - Both just initialize the value inline, no heap allocation, no null possible

  So the transpiler needs to know whether a type is a class or struct to decide:
  - class: new ClassName() → ClassName::Create() (factory that allocates storage)
  - struct: new StructName() → StructName() (just calls constructor)

The transpiler already keeps track of classes vs. structs, so tomorrow I need to add a `Create` (or maybe I'll call it `New`) static method on every wrapper class, and then use this when transpiling `new Foo`.
