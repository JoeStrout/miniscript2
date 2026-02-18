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

The local var problem was easily fixed, along with a couple other minor features; the VM test now runs and passes the tests.

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

1. I get a bunch of "unused parameter" warnings in Parselet.g.cpp; I might be able to fix those by simply omitting the parameter name where we aren't actually using it.  (...UPDATE: nope, that's not even valid C#.  Fixed instead by suppressing this warning for any file that uses core_includes.h.)

2. One of the unit tests (`build/cpp/miniscript2 -debug`) is failing because the number-to-string function is returning "42.000000" instead of "42".

Both issues now fixed.  We have C++/C# parity again!

So, now I've started on code generation.  Using a modified Visitor pattern to let the CodeGenerator iterate through a abstract syntax tree, and output either assembly code or bytecode.  I've left some ToDo's in the code, and haven't even tried to transpile it yet, but it's off to a decent start.


## Feb 01, 2026

The code generator is looking really good in C#.  There are a few things that are still placeholders, like variable handling and the implementation of the `not` operator (which probably needs to be a new opcode), but the basic structure looks really sound.

One thing needed: better tracking of free registers.  Right now, the way we track it only works if you release registers in inverse order of when they were acquired.  ...Now fixed.

So, gee.  What now?  We have (in theory) a complete pipeline from source code to executable bytecode, but currently only accessed piecemeal, in unit tests.  I guess the next step is to put it all together.  The main program should accept a .ms file, compile it, run it, and print the output.  Then we can start building a test suite.

While working on that, discovered a snag.  We have interfaces defined for ICodeEmitter and IASTVisitor.  Both of these are implemented by reference types.  That works as far as values passed into a method (where the transpiler sneaks a `&` onto the parameter type), but it does not work for local variables or class fields.  Both the App class, and the CodeGenerator class, were trying to keep a field of type ICodeEmitter.

So, I'm changing ICodeEmitter to CodeEmitterBase, and making it a true base class.  Then it can contain the storage (smart pointer), and everything should Just Work™️.   ...OK, that's all sorted out now.

So, the miniscript2 executable now accepts a .ms file (OR a .msa file), which it compiles and runs and prints the output of.  Or you can give it some code after a "-c" argument, just like Python.  Neat!

However this has exposed some bug in the C++ code; it's now segfaulting inside the unit tests.  lldb shows that this is inside Parser::GetPrecedence, which has a bad pointer (value 0x18, probably some offset from a null pointer).

Ah.  Turns out to be a flaw in the way wrapper classes were generated in some cases; the subclass had its own `storage` pointer, shadowing the base class one, which never got initialized.  I've refactored previously duplicate and twisty logic for those wrapper classes into a single, much neater `prepareRefClass` method in the transpiler, fixed the flaw, and now the C++ code is passing all tests.  Both builds now execute code (consisting of simple numeric expressions) given via -c or a .ms file.


## Feb 03, 2026

Since we have code compiling & execution now, it's time to start on the integration test suite!  Rather than try to run the MiniScript 1.x TestSuite.txt, which would contain tons of stuff we don't support yet, I'm starting a new one.  I'll try to be more thorough and complete with it this time around, too.  This one is at tests/testSuite.txt, and it will be run automatically whenever miniscript2 is started with the -debug flag.

Also today: adding support for the `and`, `or`, and `not` operators.

## Feb 05, 2026

Things are progressing really well.  I've added support for `print`, and both the test suite runner and the regular program runner now compile multi-line programs.  Now working on variable support.  ...Basic variable reads and writes are working now (using `LOADC`, which should call a function it is one, though we have no easy way to test that yet).

Near-term ToDo list:
- handle semicolons in addition to newlines for separating statements
- fix how many digits are printed on a number like 10/3; currently it's way too many
- handle Address-Of reads like `@x` (should use `LOADV` instead of `LOADC`)

## Feb 06, 2026

Handling semicolons by having the lexer return them as EOL, so to the parser, it's exactly the same as '\n'.

Then I noticed today that the way we're compiling function calls is a bit wasteful; when I compile `print 42`, it currently generates:                                                              

  Instructions (4):
  0000:  0201002A | LOAD    r1, 42
  0001:  01000100 | LOAD    r0, r1
  0002:  3E000000 | CALLFN  0, k0
  0003:  40000000 | RETURN
                                                                                                                  
We're loading 42 into r1, and then copying r1 into r0.   So, I'm adding a `CompileInto` helper method in CodeGenerator, which keeps track of what register we want the result of the node we're compiling to go into.  This should be helpful not only in the case of function calls, but probably other places where we want to compile something directly into a particular register (e.g. a variable).  After that optimization, `print 42` now produces:

  Instructions (3):
  0000:  0200002A | LOAD    r0, 42
  0001:  3E000000 | CALLFN  0, k0
  0002:  40000000 | RETURN

Much better!

I've also standardized the conversion of numbers to strings, using essentially the code from MiniScript 1.x.  So from yesterday's to-do list, that just leaves handling AddressOf (non-calling) variable reads like `@x`.   ...And now that's done too.

So, since we have a little more time today, I'm going to implement `while` loops (our first flow control!) next.   ...And done!


## Feb 07, 2026

Today I'm just adding more parsing of various control structures: `if`, `break`, `for`, etc.  I also found some repetitive code that was a good candidate for DRYing out.


## Feb 09, 2026

Added support for functions today.  These hook into the parser as expressions, so ought to be usable everywhere an expression is allowed, though I haven't thoroughly tested that yet.  Near-term to-do list:

- Confirm that we can use functions in list literals, map literals, and as arguments to other functions.
- Add support for default parameter values.  Try to do this in a way that allows any expression, as long as it simplifies to a constant.
- Move on to dot syntax (working towards OOP features).


## Feb 10, 2026

It occurred to me this morning that in MS2, we could allow list and map literals as default values -- they just need to be (automatically) frozen.  So, it might be just about time to add support for the freeze feature.

But, I've just noticed that we don't support line continuation where we should.  So, implementing that first.   ...Done.

So now I'm implementing frozen lists/maps, but it has exposed another to-do item:
- indexed assignment (e.g. x[2] = 42)

To make error reporting for things like frozen values easier, I added a thread-static ActiveVM, so any code called from the VM can get to the VM (and therefore to its error pool).  All this is working fine in C#, but it needs some work in C++.  The H/CPP guards have gotten a little thorny.  I have to quit for now, but when I get more time I need to sit down and carefully sort those out.


## Feb 11, 2026

OK, I added support for thread-local static variables to the transpiler, and cleaned up the static _activeVM field so that it actually uses our wrapper pattern instead of fighting it.  On the C++ side, runtime errors are routed through a little callback (see vm_error.h/.c) which maintains the isolation between low-level Value code, and higher layers like the VM and error systems.  All working nicely now!

My to-do list appears to be growing, though.  Let's recap:

- Support indexed assignment (e.g. x[2] = 42)
- Add support for default parameter values.  Try to do this in a way that allows any expression, as long as it simplifies to a constant.  Default list/map literals should be frozen.
- Move on to dot syntax (working towards OOP features).
- Add a REPL for more interactive testing.

I'm actually a bit torn on that last one -- it would be handy, but at the same time, it would reduce the pressure to constantly add more integration tests, which is what I *should* always do.  So, hmm.  Going to think about that.


## Feb. 15, 2026

Indexed assignment works now.

And, so do default values, including _computed_ default values!  As long as it simplifies to a constant, you can do stuff like `function(x=6*7)`.  You can also use maps and lists as default values; they appear frozen (immutable).  Huzzah!

Now working on dot syntax... and done.  That's everything from the to-do list in the last entry except a REPL, and I'm still thinking about that one.


## Feb. 16, 2026

Added the `new` operator.  In testing that, I discovered that we had not yet put in proper comparison semantics for lists and maps; that's fixed now too.  (Though I'll need to return to that at some point and put in guards against infinite recursion.)

Tackling `isa` next; for now, only looking at maps (i.e. we don't yet have the built-in maps `number`, `string`, etc., and that's fine for now).   ...And, that's done too.

So, that brings us now to `self` and `super`.  These are a bit tricky.  `self` needs to refer to the map the function was invoked on.  It will need to be set up as part of the call context, maybe like an implicit parameter.  Note that MiniScript 1.3 does this for index syntax as well as dot syntax:

```
> A = {}
> A.f = function; print "self: " + self; end function
> A.f
self: {"f": FUNCTION()}
> a = new A
> a.f
self: {"__isa": {"f": FUNCTION()}}
> a["f"]
self: {"__isa": {"f": FUNCTION()}}
> f = @a.f
> f
Runtime Error: Undefined Identifier: 'self' is unknown in this context [line 1]
```

`super` also needs to be set up with the call, but it refers to the parent of the map the function was found on (which may not be the same as the one it was invoked on).

So, do we just store these as two additional slots on the stack?  Or do they belong somewhere else, like in CallInfo?  The simplest solution seems to be to put them on the stack, but only when they are actually referenced by the code; we can determine this and assign slots as we compile, just as we do with local variables.  

In the course of implementing that, we found some other rough edges (map keys were not properly comparing strings bigger than tiny-string size, etc.).  Fixed.  Also, refactored CodeGenerator to keep a pending FuncDef and fill it in as it goes, rather than creating it only at the end (requiring a bunch of extra state to keep all the data we need).


## Feb 17, 2026

Taking stock of what's not yet working (or maybe working, but not in the test suite):

✔️1. String subtraction (-) — the "chop" operator
✔️2. String indexing (s[i]) — get single character by index
✔️3. String slicing (s[i:j]) — substring extraction
✔️4. List slicing (lst[i:j]) — subset of a list
✔️5. Doubled quote for literal quote — "say ""hello""" → say "hello"
  6. @ operator — function reference without invoking
✔️7. List + — concatenation ([1,2] + [3,4])
✔️8. List * / / — replication/division
✔️9. Map + — merging maps
  10. for over a map — iterating map keys
  11. for over range() — (depends on range intrinsic)
  12. Closures capturing outer variables — non-self outer-scope capture
  13. return with no value — bare return (implicit null)
  14. Recursive functions
  15. null in expressions — comparisons like x == null, null arithmetic
  16. true / false as standalone values — beyond just default params
  17. Chained comparisons, e.g. a < b <= c

  
## Feb 18, 2026

Tackling slice syntax today.  This has involved two new opcodes:

| Mnemonic | Description |
| --- | --- |
| LOADNULL_rA | R[A] := null (no constant pool lookup needed) |
| SLICE_rA_rB_rC | R[A] := R[B][R[C]:R[C+1]] (slice; end index in adjacent register) |

That last one is a bit unusual because it requires the slice start and end arguments to be in adjacent registers, similar to a function call.  But we needed 4 arguments total (including the destination register), so there was no simpler way to do it.  In practice it shouldn't be a big deal since we allocate registers sequentially anyway.

Also dealing with string subtraction, and handling of doubled quotes in the lexer.  ...And, list concatenation and replication.  Just checking things off of yesterday's list.


