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


## Jan 01, 2025

I'm still working on getting the new transpiler to do all the things the old one could do.  The rewrite caused quite a lot of regressions, as expected.  But, the new code structure is much cleaner, and is remaining so even as these additional features are added.  So it will all be worth it in the end.

The biggest problem I'm still struggling with is correctly telling when to change "." to "::".  The new transpiler builds a symbol table which is supposed to handle this, but it's not working in all cases.
