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


## Jan 09, 2026

Wrapper classes now get a static `New()` method, which creates a new storage instance and returns it as a wrapper.  And the transpiler now uses that when it hits a pattern like `new Foo()`, where Foo is such a reference type.  The transpiled layer2 FuncDef test now passes!  🥳


## Jan 10, 2026

Starting on layer3 tests, which will include Assembler and Disassembler.

This has exposed that our CS_List, CS_String, and CS_Dictionary classes need to have this same `New` factory method, for the sake of consistency.  

## Jan 12, 2026

Disassembler test is passing; working today on Assembler test.

One issue that may need revisiting: static methods in a reference type (like Assembler.GetTokens) are currently transpiled such that the real implementation is in the storage class, and the wrapper class calls through to that.  This works but seems unnecessary; since it's a static method, and can't reference any of the storage data anyway, we could just implement it in the wrapper class.  Of course the wrapper is thin and it probably compiles down to the same thing in this case, so it's not a high priority.

Meanwhile, Assembler.cs has a few remaining transpiler issues.  It's getting close, though!

## Jan 13, 2026

All layer3 transpilable tests are now passing in both C# and C++.  Note that one of the Assembler tests checks error handling, which works, but the Assembler itself is printing an error message to the console, which can be confusing when you run the test.  I'll probably revisit that at some point, but it's harmless so I'm going to press on for now.  We're nearly to the point of being able to transpile and run the main program.  Recall that we do this with (from the project root):

	tools/build.sh cs
	dotnet build/cs/MiniScript2.dll path/to/inputfile
	
	tools/build.sh transpile
	tools/build.sh cpp

...but the C++ code isn't compiling quite yet; we have more work to do in both App and VM.  Probably we should make one more test framework, for the VM.  Making that as layer4.

Transpilation of VM is making progress.  I've made CallInfo a struct (rather than a smartref-managed class); but it seems like the transpiler is failing to recognize that `new CallInfo(args)` in C# should be just `CallInfo(args)` in C++, for structs like CallInfo.

It's also somehow failing to grok what mainFunc is (declared as `FuncDef mainFunc` on lin 88), and so making lots of `::` errors.

## Jan 14, 2026

Fixed the problem with CallInfo; we weren't handling the case of a known struct, but only known (wrapper) classes and unknown types.  Structs are now handled just like unknown types (remove `new`).

And the problem with `mainFunc` was just that I had a pointless construct:
	FuncDef mainFunc = null; // CPP: FuncDef mainFunc;
Oh wait -- that's not pointless; C# requires it be assigned a value before it is used.  Hmm.  But it *should* be pointless.  OK, added a wrapper ctor that takes a nullptr, which enables this sort of assignment (and transpiler now converts `null` to `nullptr` too).

So, making progress.  Still have some issues:

1. Our usual transformations are not applied on a local var declaration line, like
		Value locals = frame.GetLocalVarMap(stack, names, baseIndex, curFunc.MaxRegs); GC_PROTECT(&locals);
or
		Value outerVars = funcref_outer_vars(localStack[BytecodeUtil.Cu(callInstruction)]); GC_PROTECT(&outerVars);

This is causing grief.  We need to do those same transformations, and *also* add the GC_PROTECT call.

2. I need to think more carefully about local Values declared inside code blocks, like Value locals in VM.cs:460.  And in particular, what if it's in a loop?  What's the proper GC pattern in this case?

## Jan 15, 2026

The local var problem was easily fix, along with a couple other minor features; the VM test now runs and passes the tests.

But it looks like I do indeed have to reconsider how I protect local values in a loop.  GC_PROTECT pushes an entry onto the shadow stack.  If I call that repeatedly, the shadow stack grows more than it should.  Doing this in the main dispatch loop, a MiniScript program could even cause the shadow stack to overflow.

Claude suggests:

  1. For the VM specifically: The VM's stack and names arrays should themselves be GC roots. Values stored there are protected because they're reachable from those roots. Local variables that are just copies from the stack don't need individual protection - they're temporary and no GC should happen while we're mid-opcode.
  2. For loops in general: Either:
    - Use GC_PUSH_SCOPE() / GC_POP_SCOPE() around the loop body (expensive but correct)
    - Or declare protected variables with GC_LOCALS() outside the loop and reuse them
  3. For the transpiler: It needs to be smarter about when to emit GC_PROTECT. Simply protecting every local Value is both incorrect (in loops) and inefficient.

After reflection, I've decided to tackle it this way:

1. Disallow declaring a local variable inside a loop; they must be declared right in the method body.  I think I can enforce this in transpiler by checking the indentation level (since we already require classes to be flush-left).  I'll update the VM code to follow this pattern, declaring all locals at the top.

2. Give the GC system a "mark callback" list, invoked in gc_mark_phase.  The callback is responsible for marking any Values not otherwise protected from collection.

3. Add code in VMStorage to register such a callback on init, and deregister it upon shutdown.

Items 2 and 3 are done (yet untested), but I still need to do item 1.

## Jan 19, 2026

Implemented item 1 above, i.e., we now check for `Value` declarations indented more than two levels, and updated VM.cs accordingly.  VM now transpiles and passes again.  But it's still not complete, because we're not calling GC_PUSH_SCOPE and GC_POP_SCOPE in the appropriate places.

That's now fixed too; the transpiler keeps automatically emits GC_PUSH_SCOPE before the first local Value, and when it has done so, emits GC_POP_SCOPE before any `return`.  Tests are still passing, and now the GC management looks correct.

So the next step is to turn to the main program (App.cs), get that transpiling, and get the whole program building again.  Then we can finally get back to the compiler portion, which this time will be a hand-written Pratt parser rather than a using a parser generator.

## Jan 24, 2026

The transpiler seems to be working pretty well now, though there is one known issue: any C# code inside a CS_ONLY block is skipped entirely, which means that we don't get symbol information for the classes defined separately, like Value, ValueMap, and ValMap.  This doesn't currently seem to be causing any grief, but I expect I'll need to deal with it at some point.

However today, I really want to get the full app compiling again.

Making progress on that.  Lexer.cs was added somewhat recently to support the auto-generated parser.  The latter is out, but I'm keeping the lexer as we're going to need it for our Pratt parser too.  It took a bit of massaging to conform to our CS coding standards, but it's compiling cleanly now.  However, its handling of Unicode is rather crude and probably incomplete, as it's passing characters around as Char (which is the same as `char`), and in C++, that's an 8-bit value.  It doesn't matter in most places, as none of our keywords, operators, or syntax bits are non-ASCII characters.  But we'll need to be careful with identifiers, strings, and maybe even comments.  At the very least, I'll want to come back with a solid test suite for these.

But, again, I'm focused on getting the full app compiled at all this weekend, so the next challenge is the UnitTests module.  This is using syntax which our transpiler does not properly handle:
		new List<String> { "LOAD", "r5", "r6" }
This should transpile to
		List<String>({ "LOAD", "r5", "r6" })

## Jan 27, 2026

The above feature (list initializers) is now handled.  I think our second-generation transpiler now does everything the first one did, and more; I'm able to transpile the complete current MiniScript 2 prototype, and the C++ version builds and tests clean.  (I haven't tried *very hard* to find GC bugs yet, but at least the basics are there.)

I've also updated the build scripts so that the output (executable) is called `miniscript2`.  Obviously this is only during development; once we get close to a release, we'll name it miniscript, just like version 1.

So, on to the parser!  I've moved the Pratt parser prototype over from MS2Proto5, and updated it to our current C# coding standards.  That *should* make memory management for AST nodes and parselets "just work".  We have a bunch of unit tests that parse, simplify, and convert back to string, which are all passing on the C# side.

The transpilation (or something about the C# source) still needs a bit of work, though, because I'm getting errors.  The transpiler's scanning seems to have missed all the parselet types... and indeed, none of the Parselet classes are declared in Parselet.g.h at all.  ...OK, that's fixed.

There are more issues, but it's late.  I'll continue tomorrow.


## Jan 29, 2026

OK, all the transpiler issues exposed by the new parser code have been fixed, and the transpiled project builds for C++ again.  🥳  There were a number of thorny issues due to the more complex class hierarchy used for ASTNodes and Parselets (and the circular dependency between Parselets and Parser), but it seems like they've all been sorted out now.

There are a couple of minor issues to address next:

1. I get a bunch of "unused parameter" warnings in Parselet.g.cpp; I might be able to fix those by simply omitting the parameter name where we aren't actually using it.

2. One of the unit tests (`build/cpp/miniscript2 -debug`) is failing because the number-to-string function is returning "42.000000" instead of "42".

