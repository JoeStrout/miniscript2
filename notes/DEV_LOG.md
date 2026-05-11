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
✔️6. @ operator — function reference without invoking
✔️7. List + — concatenation ([1,2] + [3,4])
✔️8. List * / / — replication/division
✔️9. Map + — merging maps
✔️10. for over a map — iterating map keys
  11. for over range() — (depends on range intrinsic)
  12. Closures capturing outer variables — non-self outer-scope capture
✔️13. return with no value — bare return (implicit null)
✔️14. Recursive functions
  15. null in expressions — null arithmetic
  16. true / false as standalone values — beyond just default params
  17. Chained comparisons, e.g. a < b <= c

  
## Feb 18, 2026

Tackling slice syntax today.  This has involved two new opcodes:

| Mnemonic | Description |
| --- | --- |
| LOADNULL_rA | R[A] := null (no constant pool lookup needed) |
| SLICE_rA_rB_rC | R[A] := R[B][R[C]:R[C+1]] (slice; end index in adjacent register) |

That last one is a bit unusual because it requires the slice start and end arguments to be in adjacent registers, similar to a function call.  But we needed 4 arguments total (including the destination register), so there was no simpler way to do it.  In practice it shouldn't be a big deal since we allocate registers sequentially anyway.

Also dealing with string subtraction, and handling of doubled quotes in the lexer.  ...And, list concatenation and replication.  Just checking things off of yesterday's list.  Added map addition.

While testing map addition, I noticed something not on our list: math-assignment operators (e.g. `+=`).  And testing _that_ led to realization that we had not yet implemented `^`.  But it's actually great that we're getting down to this level of nitty gritty, because it means all the _big_ features are working!  ...All fixed now.


## Feb 19, 2026

Well that's what I get for claiming all the big features were done. 😅  Testing recursive functions led to the realization that the function-call code was only looking for the function name in the local scope... and making it use the usual variable look-up broke intrinsic functions... which led to addressing the total hack that was our approach to intrinsics up to now (a VM.DoIntrinsic method that switched based on the function name).

So, I'm now refactoring intrinsics to be something much more similar to how they work in 1.x: a table of functions, which variable look-up will check after locals, outer, and globals.  And that requires the transpiler to deal with function delegates and lambdas.  This is a lot of work.

## Feb 20, 2026

Finally finished the refactoring of intrinsics, and polished off the rough corners of the transpiler that were exposed thereby.  

So, recursive functions are now working properly.  Next we should implement commonly used intrinsics, especially `range`, so we can start writing `for` loops in a more normal way.

## Feb 23, 2026

Today I'm going to try to refactor how iteration via a `for` loop works, particularly for maps.  Right now that involves counting all the items, _and_ counting again from the beginning of the map to `i`, on every iteration of the loop.  I think we can do much better: the hidden iteration variable should reference not a 0 to n-1 "index" value, but instead the index into the hash table's `entry` array, automatically skipping over any unoccupied slots.

On the C# side, we can't do that, so I took a different approach: a cache of the key values, lazily obtained from `Dictionary.Keys` when needed by an iterator, and cleared when the map is mutated.  The iterator then just indexes into this list.  When we implement the `indexes` intrinsic, we should make use of this cache as well (a minor optimization but basically free).

Then I turned to intrinsics.  While testing string intrinsics, I realized that I had skimped a bit on Unicode support.  Fixed that by making our CS_String class embrace character-oriented indexing.  The byte-oriented methods are still there and available to C++ code, but transpiled code will naturally use the character-oriented ones, in keeping with how C# works.  This all seems to be working well now.

So most (though not all) core intrinsics are implemented now.  One caveat: we don't yet support dot syntax on these, because I don't have the little type maps that represent intrinsic types, and those are used to look up dot-invoked methods.  So we're testing things with code like `pop(lst)` instead of `lst.pop`.  I'll deal with this soon.

## Feb 24, 2026

Discovered a bit of a misstep: our ValueList struct kept its items at the end of the struct, rather than pointing to a separate place in the heap.  Great for performance, but it makes it impossible to resize a list!

ValueMap was already doing it correctly, but we had prematurely (and incorrectly) optimized ValueList.  Fixing that now.

Also: in developing and testing all these intrinsics, I see that there is some redundancy (and possibly some sub-optimal handling of character vs. byte offsets) in the string code.  We should go through that carefully at some point, DRY it out as much as possible (though keeping in mind that Tiny Strings are a common thing in Value strings), and make sure it's all as efficient as it can be.  This should include optimizing for the case of ASCII strings (where we know it's 1 byte per character).

Finally, the way we handle (internal to our own code) iteration over map values is similar between C# and C++, but just different enough to be annoying.  This needs some creative thinking and/or some transpiler love to make it more seamless.

From last week's to-do list, here's what is left:

✔️11. for over range() — (depends on range intrinsic)
✔️12. Closures capturing outer variables — non-self outer-scope capture
✔️15. null in expressions — null arithmetic
✔️16. true / false as standalone values — beyond just default params
✔️17. Chained comparisons, e.g. a < b <= c

The only one that took significant development was the last one, chained comparisons.  These required a fancier parselet and code generator.  Note that I decided to keep the MiniScript 1.x behavior, and not short-circuit when the expression has become false; this should actually be more performant in common cases, and is certainly simpler.

## Feb 27, 2026

Rounded out support for 'isa' with our new type intrinsics, so you can say "42 isa number" (etc.) now.

But we forgot to mark the new type maps as roots in the GC system, so while it's working perfectly in C#, it soon fails in C++.  That should be an easy fix, but it led to a somewhat thorny issue: the function indexes for our intrinsic functions need to be kept separate from the compiled functions.  The solution, for now, is to invalidate the type maps when we re-register all the intrinsic functions, which happens for each VM.  But this won't work when there are multiple VMs sharing the intrinsics.  I've added a note to POTENTIAL_ISSUES.md.

In addition to the type maps, we also have to mark everything in the _intrinsics dictionary.  GC systems are super annoying this way: if you miss any roots, you will eventually free something you didn't mean to, and cause failures later on.  So far our test suite has proven to be a pretty good way to trigger these failures, but we should think about doing something more systematic, like triggering aggressive GC and garbage-overwrite on a regular basis.


## Mar 02, 2026

We have almost the full language implemented at this point, and I'd like to spend a little time soon on benchmarking & optimization.  I expect that as the code base matures, we will alternate between phases of optimization, and phases of beautification (which will include gradually making the APIs match MS1 as closely as possible, and/or providing adapters for older code).  Right now we will also need to mix in completeness/correctness updates, but those should be done fairly soon.

I just had a look at our miniscript-benchmarks repo, and there are a couple of things preventing those scripts from running in MS2 right now:

1. `locals` and `globabls` are not yet implemented.
2. The `time` intrinsic is not yet implemented.

So let's tackle those today.  (At some point we'll need to also tackle `wait`, which implies reentrant intrinsics, but probably not today.)

But our intrinsics currently don't have access to the VM, which is needed for all three of these new ones.  So I'm refactoring the NativeCallback interface to take a context object (more like MS1) rather than the (List<Value> stack, Int32 baseIndex, Int32 argCount) parameters it takes now.

Though actually, now that I think about it, we probably need to keep `locals` and `globals` as keywords, and deal with them at the compiler level rather than make them standard intrinsics.  But the refactoring above will still be needed for `time` (and no doubt others to come).

While implementing `locals`, I've found myself once again annoyed by the undefined order of key iteration in maps (e.g. when converting a map to a string for a test case!).  Going to tweak the CS_Dictionary implementation so that it can iterate over keys in insertion order rather than essentially random order.  Fixing that by rewriting value_map to use a chaining algorithm, like CS_Dictionary.h (which also had a couple of bugs that broke ordering, now fixed).  

Added implementations of `outer` and `globals` now too.


## Mar 09, 2026

I found a couple of small issues in testing this morning: global variables were not being implicitly looked up, and string concatenation with lists and maps did not work.  Those are fixed.  Also, I needed to implement the `time` intrinsic.

But with all that in place, I can start running benchmarks!  `tools/build.sh cpp` already builds with optimizations on.  So:

(base) √ miniscript2 % miniscript benchmarks/fib-recursive.ms                                              rfib(30) time: 13.088
(base) √ miniscript2 % build/cs/miniscript2 benchmarks/fib-recursive.ms                                    rfib(30) time: 3.41
(base) √ miniscript2 % build/cpp/miniscript2 benchmarks/fib-recursive.ms                                   rfib(30) time: 0.833

Woo!

I had Claude make an option to do debug builds (asserts on, optimizations off) when we want them.  Usage summary:                                            
  - tools/build.sh cpp — release build (unchanged, -O3 -DNDEBUG)
  - tools/build.sh cpp debug — debug build (-O0 -g, asserts enabled)
  - tools/build.sh cpp debug on — debug build with computed-goto forced on
  - Options can appear in any order after cpp

I just have to remember to do a `tools/build.sh clean` when switching between debug and release builds.

I'm brushing off the benchmark.sh script, which compares equivalent programs in three versions of MS2, as well as MS1, Python, and Lua.  But the MS2 ones are using hand-written assembly.  We need to update that to use the same .ms files that MS1 uses (perhaps keeping the hand-written assembly in there for comparison).

But, using the .msa files for MS2, here's where we sit today:

| Benchmark           | C#        | C++ (switch) | C++ (goto) |  MS 1.0  |  Python  |   Lua |
|---------------------|-----------|--------------|------------|----------|----------|-------|
| Iterative Factorial | 3.382s    | .419s        | .333s      | 42.285s  | 2.370s   | .643s |
| Iterative Fibonacci | 3.862s    | .412s        | .356s      | 69.745s  | 2.258s   | .764s |
| Recursive Fibonacci | 3.941s    | 1.636s       | 1.717s     | 55.675s  | 1.421s   | .425s |

OK, and after fixing an issue with nested loops, I was able to get results including MS2 using source (src) files:

| Benchmark          |C# asm| C# src|sw asm|sw src|goto asm|goto src| MS 1.0|Python| Lua |
|--------------------|------|-------|------|------|--------|--------|-------|------|-----|
| Iterative Factorial|3.529s| 8.187s| .326s| .670s|  .296s |  .687s |37.194s|2.245s|.610s|
| Iterative Fibonacci|3.775s| 9.065s| .326s|1.056s|  .314s | 1.007s |67.744s|1.947s|.695s|
| Recursive Fibonacci|3.994s|12.931s|1.452s|3.314s| 1.488s | 3.444s |56.744s|1.381s|.352s|

So, we're doing well overall.  Note that the "Iterative Fibonacci" and "Recursive Fibonacci" aren't doing the same work -- so it's not actually the case that Python and Lua are faster at recursion than iteration.  Not by a long shot.  However, it does seem that we still suffer more from recursion than they do.

A major goal for MS2 was to bring our call overhead down.  Comparing to MiniScript 1, we have succeeded at that; but compared to Pythan and Lua, it seems we could still be doing better.

We spent most of the day trying a major refactoring, eliminating the CallInfo stack and moving that data (as four Values) into the regular value stack.  But this produced very minor benefits at best, and actually made the goto-src case slightly slower.  So I've reverted it.  We're going to keep some minor optimizations, but I think the next step is probably to get this running with the Apple Profiler and see where the time is going.

## Mar 10, 2026

So, profiling.  The first step is to build with debug symbols:

	cd cpp && make clean && make OPT_FLAGS="-O3 -DNDEBUG -g"

Then, I can run with `sample` and write the output to a file:

	build/cpp/miniscript2 tools/benchmarks/recur_fib.msa & sample $! 5 -file profile.txt

This profile.txt can be loaded in Instruments, or just read as a text file.  (I had to increase the size of the Fibonacci number in recur_fib.msa in order to get a good-sized sample.)  The results show that it's spending most of its time incrementing and decrementing reference counts in the smart pointers, inside VM::RunInner.

Also note that while trying things, you can get a quick check by timing a single run:

	time build/cpp/miniscript2 tools/benchmarks/recur_fib.msa 

This morning, that's reporting 3.75s for my modified recur_fib.msa script before any refactoring.  After replacing `curFunc` (a FuncDef that wraps a smart pointer) with `curFuncRaw` (a FuncDefStorage dumb pointer) in the CPP code, the same test takes 1.4 seconds.  A substantial improvement!

Then moved recalculation of local_stack into only the places where it changes (instead of on every instruction); that dropped the time for rfib(35) (via asm) down to 1.31 seconds.

And with a further optimization (functionsRaw vector storing FuncDefStorage*'s), the time comes down to 1.14s!

Revised benchmark times, after these optimizations:

| Benchmark         |C# asm| C# src|sw asm|sw src|goto asm|goto src|MS 1.0  |Python| Lua |
|-------------------|------|-------|------|------|--------|--------|--------|------|-----|
|Iterative Factorial|3.071s| 7.082s| .309s| .673s| .274s  |  .548s |36.860s |2.174s|.734s|
|Iterative Fibonacci|3.573s| 7.862s| .311s|1.001s| .259s  |  .867s |65.820s |2.289s|.705s|
|Recursive Fibonacci|3.660s|11.590s| .491s|1.532s| .409s  | 1.344s |58.154s |1.313s|.394s|


## Mar 13, 2026

Thinking about the next steps: the language is nearly complete, and the performance is quite good.  I'd like it to be better -- and there is plenty of room still for optimizations at the compiler level -- but I think it might be about time to swing back to code structure & beautification.

Another thing on my mind: we don't yet have `yield` and `wait`, which are crucial intrinsics that demonstrate the reentrant nature of the interpreter.  Supporting those will require something very much like the PartialResult struct in MS1... and so this presents a good opportunity to start moving the API (for intrinsics, in particular) in the direction of MS1.

Once yield and wait are working, a good next milestone would be converting command-line MiniScript, with all of its shell intrinsics, to MS2.  That will really show how onerous that transition is.

## Mar 16, 2026

I've refactored the intrinsic callback API to mimic that in MiniScript 1.  One difference: IntrinsicResult is now a struct, so the C# code can no longer compare it against `null`; instead it should check `done` just like the C++ code.  (Or I guess you could check whether it == IntrinsicResult::Null, which is what we will always pass in for a fresh call.)

Some things for the To-Do list that occurred to me while working on this:

- We really need to delete Value.FromString, etc., from the C# code and force use of the transpilable globals (make_string, etc.).
- That long boilerplate of forward declarations in every generated header file: we should output that to just one file (perhaps ForwardDecs.g.h), and just include that.

But also today, I really want to get wait and yield working.  I might tackle those first.

That's going reasonably well; working fine in C#, but running into circular-dependency issues in C++.  For example, I wanted to store the pending Context in VM, but Context also has a VM reference itself; this is impossible (without resorting to pointers) in C/C++.

So I'm breaking apart the parts of Context (and IntrinsicResult) that we need and storing them in VM directly, but I hate that the code is being uglified just to support this stupid C/C++ limitation.  Oh, and after all that, I still needed to pull IntrinsicResult into its own header file, as it's used as a parameter in a VM method.  Ugh.

I also hate the proliferation of little source files that we're growing here.  At some point I should do a dependency analysis, break all the dependencies we can, and try to consolidate things into a few nice neat layers (with one or a few files per layer).

`wait` and `yield` are working now, though.

OK, so I'm trying to break the dependency of Context on VM by having it take a VMRef, which the transpiler already understands is a VMStorage&.  (We use a similar pattern elsewhere with FuncDefRef.)  This is working, as a quick hack, but to make it work in production we have to deal with a few things:
- The constructor must assign the value in the initializer list, not the method body;
- Where we call it, in VM.g.cpp, we need to pass `*this` (C++ code only)

## Mar 17, 2026

Continuing trying to break dependency of Context on VM.  For the constructor, I've resorted to just having separate constructors using CPP_ONLY and H_ONLY blocks.  They're trivial, so this is not a huge deal, though it might be nice to make the transpiler do this automatically somehow.

For the call in the VM code, I just put that parameter on its own line and used a // CPP: tag.  Easy enough.

I also moved IOHelper.g.h out of the StringUtils header (and into the .cpp file), and similar for FuncDef.g.h.  ...And, more surgery of similar kind.  After all that, our revised dependency graph looks something like this:

  Level 0 — Leaf headers (no generated-header deps):                                                               
    Bytecode.g.h                                                                                                   
    LangConstants.g.h                                                                                              
    ErrorPool.g.h                                                                                                  
    IOHelper.g.h                                                                                                   
    AST.g.h           (large, self-contained node hierarchy)                                                       
    IntrinsicAPI.g.h  (i.e. Context and IntrinsicResult)
    UnitTests.g.h                                                                                                  
    StringUtils.g.h
    FuncDef.g.h
    App.g.h
    CoreIntrinsics.g.h

  Level 1:
    Lexer.g.h         → LangConstants.g.h (strong), ErrorPool.g.h (strong)
    CodeEmitter.g.h   → Bytecode.g.h (strong), FuncDef.g.h (strong)
    Intrinsic.g.h     → FuncDef.g.h (strong)
    Assembler.g.h     → Bytecode.g.h (strong), FuncDef.g.h (strong)
    Disassembler.g.h  → Bytecode.g.h (strong)
    VM.g.h            → FuncDef.g.h (strong), IntrinsicResult.g.h (strong), ErrorPool.g.h (strong)

  Level 2:
    Parselet.g.h      → AST.g.h (strong), Lexer.g.h (strong), LangConstants.g.h (strong)
    CodeGenerator.g.h → AST.g.h (strong), CodeEmitter.g.h (strong), ErrorPool.g.h (strong)
    VMVis.g.h         → VM.g.h (strong)

  Level 3:
    Parser.g.h        → Parselet.g.h (strong), Lexer.g.h (strong),
                        LangConstants.g.h (strong), ErrorPool.g.h (strong)

Much better than it looked yesterday (not shown).  The remaining includes are considered "strong" by Claude, in that they're not broken without refactoring -- but I'm much more satisfied with the layering now.

And finally for today, I moved that long list of forward declarations into its own forward_decs.g.h file.  This shortens all the other headers and makes them much neater.

## Mar 23, 2026

I intended to spend today doing code inspection and clean-up.  But I ended up doing more of pushing the APIs in the direction of MS1, starting with TextOutuputMethod (including the addLinebreak parameter).  This uncovered a lurking bug in the VM, where we were storing the `outer` varmap in the wrong call frame.

That's fixed now.  But I then realized a major missing piece, which makes some of that clean-up possible: the Interpreter class.  This is also important for making the MS1 -> MS2 transition tolerable for embedders; the Interpreter is the main interface between MiniScript and the host app.

But getting this to transpile cleanly to C++ was a major PITA, because of circular header hell: VM includes an Interpreter reference, and Interpreter includes a VM reference.  Both are necessary.  To make it compile in C++, I made the VM's reference actually an InterpreterStorage* in C++, and have Get/Set methods to mostly hide that ugliness.  But the usual wrappers of those methods are inline in the header, which still has the same problem.  So I had to add a NO_INLINE feature to the transpiler to force those method definitions into the .cpp file.

Eventually, though, all the issues were solved.  Compiling cleanly for both C# and C++, and we now have an Interpreter (and fixed `outer` variables).

## Mar 24, 2026

Today I asked Claude to look for code duplication and other "bad smells" in the C# code.  The worst offenders it found were in Assembler.cs.  I don't care about that one a whole lot -- it probably won't actually be included in the final shipping version of MiniScript 2, or it'll be some optional extra install for those with some weird reason to need it.  But, if we can slim it down by 200 lines or more, then why not.

It found some code duplication in VM.cs.  This is stuff I was already aware of, but it's in the hot path, so we have to be very careful — any method we extract will need to be inlinable.  But if we can manage that, then it may be worth doing.  A bigger win would probably be to tame that nest of Value declarations at the top of VM.RunInner; we certainly don't need that many.  The big decision there is: do we try to keep useful names, or do we just call them val1 ... valN?  (If we do the latter, we probably won't neem any more than val3 or val4.)

CodeGenerator.cs also has a couple of opportunities for eliminating duplicate code.

Assembler and CodeGenerator have been deduplicated; and I manually reduced the local Values in RunInner, by making heavy use of names based on the opcode register they come from: valA, valB, and valC.  There are a couple of others I still needed, but it's dramatically better than it was (though this does make the code slightly harder to read).

## Mar 30, 2026

I think it's time to add the REPL.  We already have stub REPL() and NeedMoreInput() methods. The challenge is threefold: (1) detecting when user input is incomplete (open blocks), (2) compiling each REPL entry as a fresh @main while preserving global variables across entries, and (3) supporting implicit output for bare expressions.

Each REPL entry compiles as a new @main function, but all entries share a persistent globals VarMap that carries variables forward. The parser gains a "need more input" signal (distinct from syntax errors) for incomplete blocks. The Interpreter orchestrates the lifecycle: accumulate lines, compile, run, gather globals, report implicit output.

The trick is to keep the global VarMap around, and in sync with the new variable registers each time we make a fresh main.  We'll add a new `Rebind()` method that re-attaches the new registers to the existing map.  There are three spots that access `callStack[0].GetLocalVarMap` to look up globals (these are LookupVariable, the OUTER_rA opcode, and the GLOBALS_rA opcode).  At each of those, we'll check for `ReplGlobals` and use it if found.

Resetting the VM requires care in this case: we must really do a *partial* reset, keeping the functions list from before, while creating new stack and name lists -- but then rebinding the persistent globals map to these new lists.

It's mostly working now, but there is one problem: as we compile code freshly for each input, every time we define a function, it gets a funcIndex of 1.  You can see this in CodeGenerator.Visit(FunctionNode node), which sets the funcIndex to _functions.Count.

This relates to the choice made a while back to add all the intrinsic functions *after* the user-defined functions, which works only as long as there aren't multiple VMs sharing them (or sharing code compiled at different times), because then they might get different function indexes.  The whole approach is fragile, and now (with the REPL) unworkable.

So what we should do instead is: add the intrinsics *first*, when a VM is initialized.  And then give CodeGenerator an offset (`firstFunctionIndex` or similar) to add to _functions.Count for any functions that it generates.  This way, even on a non-REPL run, it can stick the user functions after the intrinsic functions; and on a REPL run, it can also avoid reusing indexes in use by previous user functions.

...OK, that refactoring is done, and it does seem to solve the problem.  We now have a working REPL in both C# and C++.

## Mar 31, 2026

Today I'm going to refactor the App code a bit; much of it predates the Interpreter class, and so is working harder than it needs to.

OK, so that's done and it helps quite a bit.  App.cs is now much shorter and cleaner, and it's properly exercising Interpreter.

Now I'm going to indulge in a little quality-of-life enhancement over MS1: I'm going to style (color) the text in the REPL.  We'll have the prompt in gray, the user's input in normal, and the output in bold.


## Apr 06, 2026

I'm adding metadata to functions today, accessed through a new `@note` intrinsic.  Example (via the REPL):

> foo = function
>>> "Returns the ultimate answer to life, the universe, and everything."
>>> return 42
>>> end function
> info(@foo)
{"type": "funcRef", "__idx": 54, "name": "foo", "note": "Returns the ultimate answer to life, the universe, and everything.", "params": []}

`info` returns a frozen map, and in the case of a function, that includes `name`, `note` (when the first statement in the function evaluates to a string literal), and `params`.  The `note` expression supports string constant folding, which will be most useful if you
want to break your note up onto multiple lines.


## Apr 13, 2026

Today we're going to add the `error` type to the language.  This is probably going to end up being the major new language feature in 2.0, but I think it's totally worth it.

Claude wanted to add `Pure` and `AllowErrors` fields to the Intrinsic struct, and then wrap every intrinsic call in a lambda method that checked the arguments, in order to unify the error handling.  But I don't want the extra data or the extra (wrapper) function call, so I took that out.  It will be the responsibility of each function to check its arguments and handle appropriately.  Yes, that's a lot of repeated boilerplate, but I think it's worth it in this case to give each function author the maximum flexibility to handle errors as needed, and avoid the extra overhead.

The C# code is basically working now, and it transpiles, but does not yet build for C++ because we need to implement make_error, error_message, error_inner, error_stack, and error_isa (as well as the error struct itself, and associated GC integration).

That's all fixed now, and it's working in both C# and C++.  However I find myself often making this mistake:

> e = error("Doh!")
Runtime Error: Too many arguments: got 1, expected 0
> e = err("Doh!")

I think the distinction between `error` (the type) and `err` (the factory function) is necessary, but unfortunate.  Maybe we can make it easier by elaborating the error message for common cases?  In this case, it could add something like "(to create a new error object, use `err(msg)`)".

Another refinement to consider: the last error should be stored on the VM, and accessible not only to internal code (to see whether we are in an error state), but also to users via some new intrinsic (`lastError` or `error.last` or something).

That's it for today, but next time, we should consider the above refinements, and then also implement source-code location tracking (probably as some array parallel to the bytecode in each function).  That will enable us to implement the stackTrace intrinsic, and also attach a stack whenever an error is created.


## Apr 14, 2026

Today I'm adding tracking of source code files and line numbers.  Line numbers are stored (RLE compressed) in FuncDef next to the bytecode.   I've implemented the `stackTrace` intrinsic, and it seems to be working so far.

So, the next step is to attach a stack trace to errors when they are created.  ...And done.

So now I'm examining the overall error infrastructure.  I think we over-engineered a bit with ErrorPool, because all I really want is to track a single error (the first one encountered in any compilation or run).  And at the same time, we didn't provide enough helpful infrastructure for reporting common types of errors.  I'm going to rework all that a bit, based now on error values.


## Apr 27, 2026

Error infrastructure has been reworked; ErrorPool is no more.  Much better.

Today I'm doing some work on UX for the C++ command-line build (worrying less about C# here, since it's unlikely anyone will actually use the C# command-line build except for testing/debugging of MiniScript itself).  First up: line editing.  Again bundling the editline library, as we did in MS1.  This provides history and gnu-style editing.

The next level (which should apply to both C# and C++) is visible input/output history.  Here's how it should work:

- We keep two lists, accessible within MiniScript itself as `_in` and `_out`.  `_in` tracks user input (strings); `_out` holds implicit output (any type).
- The input prompt looks like ` _in[0]: ` for the first input, with `0` replaced with the current length of the input list.  In a more-input-needed context, the prompt is `   ...: `; all lines of additional input get concatenated with the base line.
- Output is prefixed with `_out[0]: `, with `0` showing the index of the last output.  Note that if the implicit output is null (or there is an error), it's not printed.
- These prefixes are drawn in the subdued style; user input happens in the normal style; and output is drawn in the strong style (defined in IOHelper).
- If the user input starts with '!', this is a special history metacommand, such as:
  - `!123`: repeat input 123
  - `!-4`: repeat the input 4th from the end
  - `!?`: show last 15 inputs, with their index numbers
  - `!?20`: show last 20 inputs, with their index numbers
  - `!? foo`: show last 15 inputs that contain "foo"
  - `!?20 foo`: show last 20 inputs that contain "foo"
- A `reset` intrinsic will reset all global variables, and also clear out the _in and _out lists.

It works!  Pretty nice.  I do wonder a little bit if it's less friendly than the simple '>' and '...>' prompts in MiniScript 1.x; I also worry a little bit about making the examples in _Learn To Code in 30 Days_ a little bit off.  But the latter could be addressed with an insert or erratum; and as for the former... well I'm not sure.  If it's an issue we could have a switch or environment variable to toggle the simple and advanced prompts.  But I dunno, I think it's fairly unobtrusive and probably fine.

## May 1, 2026

Next week I'm planning a couple of optimization tasks:

1. I want to measure whether it is actually worth having a separate `int` type under the hood, or whether all numbers should be `double`.  I suspect the latter, as it will allow us to more easily do stuff like:

2. I want to add a couple of bit flags to the list type, including `allNumeric`.  That flag will be updated whenever we easily can; it won't be guaranteed to be correct if for example you add and then remove a string to an otherwise numeric list.  But when the flag is set, we can take a faster path, maybe even do vector operations where hardware supports it.

3. Another flag can indicate a *computed* list, where what we actually store is start, end, and step; then the length and values by index are computed on the fly.  This will be really handy when somebody does something like `for i in range(1E9)`.  We'll be able to maintain this form when the user deletes the first or last element; and *maybe* when inserting a new first or last, if it happens to match the pattern; any other mutation will just materialize it into a regular list.  (Way better than Python!)

Unrelated, but something to think about for the future: apparently Zig can automatically generate bindings to C libraries on the fly, like:

```
// build.zig
const raylib = b.addTranslateC(.{
    .root_source_file = b.path("src/raylib.h"),
    .target = target,
    .optimize = optimize,
    .link_libc = true,
});

exe.addImport("raylib", raylib.createModule());
```

That's a cool trick, and something we should think about including in command-line MiniScript (and related builds) at some point.

## May 2, 2026

One of our new benchmarks uncovered a bug in the VM: when an inner function referenced a global variable, it would not find it (and, another bug: RaiseRuntimeError caused the VM to exit cleanly but never actually reported the error).

The ultimate cause turned out to be inconsistency in how we were treating `callStack` in the VM.  It was a stack of *return* frames, not execution frames.  So GetLocalVarMap would cache the varmap representing the locals for frame 1 in frame 0.  But that left us no safe place to store the globals (it was also using frame 0).

So we're factoring it so that callStack now represents execution frames, and callStack[0] is the global (main program) context.

Then I need to remember to fix the error reporting too.

## May 7, 2026

I've started documenting major architecture decisions in ADRs (in notes/adr).  ADRs are _meant_ to be written when they are made, not retroactively, but I figure better late than never.

The first two ADRs cover the decision to transpile C# to C++, and the decision to use NaN-boxing for our Value type.  But reviewing the code and consequences for the latter has turned up an implication I had not fully grasped: in our C# version, we have effectively disabled GC for all MiniScript objects.  This is because the NaN box can't contain a true reference; it can't contain a pointer (even if we could get a pointer to an object, C#'s GC is liable to move objects in the heap); and so it contains just an index into a separate Handle array.  This Handle array must contain strong references to the objects, since it will often be the _only_ reference that C# sees.  And we have no way of clearing it out.

Possibly we could periodically sweep our Handle array.  This would be similar to the mark-and-sweep operation the C++ GC does.  It would require knowing all the roots, just like the C++ code.  Unlike the C++ code, we would not have to do this at allocation time; we could have somewhat more control over when it's done (for example, only within a `yield` or `wait`).

If the periodic sweep does not work, or is too expensive, then we might have to instead change our Value representation in C# to something like a union struct, so that C# can see the references and manage GC normally.

I consider this a serious issue, and a high priority to tackle.  Probably right after deciding whether to keep ints as a separate type.


## May 8, 2026

Two updates for today:

1. I thought all night about the GC issue above, and came up with a plan.  It boils down to: we do our own mark & sweep GC only for certain MiniScript types (list, map, string, error, and "handle", meaning wrapper for some native object) -- and we do it for both C++ and C#.  I've prototyped this in a new MS2Proto2 subproject, and it seems to work fine, with relatively simple code.  So we'll probably move to that soon.

2. Meanwhile, I had Claude remove `int` as a separate numeric type, and ran benchmarks of that vs. the previous `int`-tracking code.  See the results in the [ADR](adr/0003-no-int-type.md).  It turns out there is no performance difference between having it or not, but _not_ having `int` makes the code shorter and simpler, so we're going with that.

And, attempting to run the benchmarks brought to the forefront a couple of bugs:

- Runtime errors are not displayed when running a MiniScript file, as noted on May 2nd.
- Our `locals == globals` comparison now returns false, probably as a result of the May 2nd shenanigans; we need to spend a day carefully digging into how locals/globals/outer are handled.
- We don't yet have `import` nor any shell intrinsics like `file`.  Not really a bug; technically these aren't even part of the MiniScript core.  But they are needed to run some of the benchmarks, so we should add them soon.


## May 9, 2026

Fixed the unreported runtime error.

Also, added a command completion script for tools/build.sh, which will be a minor QOL improvement for me (I type things like `tools/build.sh transpile` a lot).

Then, spent most of today reworking how values (especially lists and maps) convert to strings, to mimic MS1's behavior.  This involved a fair bit of new plumbing, since those conversion methods need a reference to the VM so that we can look up short names in the globals (and intrinsics).  But it seems to be working nicely now.


## May 10, 2026

Today I'm working no the major refactoring to put in the new GC system (see the [ADR](adr/0004-GC-system.md) and [MS2Prototypes/GCApproach2](https://github.com/JoeStrout/MS2Prototypes/tree/main/GCApproach2).

It's a big job, so I'm taking it in stages:
- implement GC system in C#
- replace GC system in C++
- add support for string interning for (6 <= length < 128) strings
- add support for handle (native object wrapper) types

And when that's all done, I really need to pause and go through the code looking for bad smells.  I suspect we've accumulated a few by this point.  For example, the apparently redundant macros in GCItems.h.

Also: during this refactoring, the meaning of "interned string" has changed: it now involves only a side-table guaranteeing quick comparisons, but still requiring those strings to be marked and swept.  I'm not 100% convinced that is the right thing to do, and need to look into this further.  Might need to profile it both ways to be sure.

...OK, after more careful thought I've written up [ADR #5](adr/0005-string-interning.md), proposing a hybrid approach: interned strings will avoid collection in the usual case, but can still be collected at more rare "full GC" times.



