## Overview

The VM in this prototype is register (rather than stack) based.  Instructions are fixed-width, 32-bit values, with 8 bits for the opcode and 24 bits for the operand(s).  Our instruction set slightly favors speed over minimalism.

## Registers

## Opcodes and Operands

Our internal opcode names include a verb/mnemonic, and a description of how the three operand bytes (A, B, and C) are used.  This allows us to overload the same verb with different variants that use the operands in different ways, and make it easy to remember what's going on with each one.

### Basic loading and data handling

| Mnemonic | Description |
| --- | --- |
| NOOP | do nothing (but do it very quickly) |
| LOAD_rA_rB | R[A] := R[B] (C unused) |
| LOAD_rA_iBC | R[A] := BC (16-bit signed value) |
| LOAD_rA_kBC | R[A] := constants[BC] (load from constants table) |
| LOADNULL_rA | R[A] := null (no constant pool lookup needed) |
| LOADV_rA_rB_kC | R[A] := R[B], but verify that register B has name matching constants[C] |
| LOADC_rA_rB_kC | R[A] := R[B], but verify name matches constants[C] and call if funcref |
| LOADV_rA_rB_rC | as LOADV_rA_rB_kC, but the expected name comes from R[C] |
| LOADC_rA_rB_rC | as LOADC_rA_rB_kC, but the expected name comes from R[C] |
| FUNCREF_iA_iBC | R[A] := make_funcref(BC) (create function reference to function BC) |
| ASSIGN_rA_rB_kC | R[A] := R[B] and name[A] := constants[C] (copy value and assign variable name) |
| NAME_rA_kBC | name[A] := constants[BC] (assign variable name without changing value) |
| CHKNAME_rA_kBC | error unless name[A] == constants[BC] (require the register to be holding that variable) |
| LIST_rA_iBC | R[A] := new list with capacity BC |
| MAP_rA_iBC | R[A] := new map with initial capacity BC |
| PUSH_rA_rB | push R[B] onto list R[A] |
| INDEX_rA_rB_rC | R[A] := @R[B].R[C]: the same lookup as METHFIND (error fields, `__isa` inheritance, type-map fallback, numeric index on lists and strings), but sets no pending self/super and never invokes. Emitted only for `@x.foo`; `@x[k]` compiles to IDXGET, since bracket access never invokes anyway |
| IDXSET_rA_rB_rC | R[A][R[B]] := R[C] (set element R[B] of list R[A] to R[C]) |
| SLICE_rA_rB_rC | R[A] := R[B][R[C]:R[C+1]] (slice; end index in adjacent register) |
| LOCALS_rA | R[A] := the VarMap for this frame's local variables (created on demand) |
| OUTER_rA | R[A] := the VarMap for the enclosing scope captured at definition time, or the globals map if there is none |
| GLOBALS_rA | R[A] := the globals map |

### Globals

Global variables are slots in the interpreter's `Globals` table, not registers;
see [GLOBALS.md](GLOBALS.md).  The BC operand of these three opcodes is an index
into the *global-reference table* of the enclosing FuncDef (`GlobalNames` /
`GlobalSlots`), which is a separate table from the constant pool — the
disassembler writes those operands as `gN` rather than `kN`.  The reference is
resolved to a slot number the first time the function runs against a given
namespace, and re-resolved if it is ever run against another one.

`GLOADC` and `GLOADV` are emitted for any *free* name — one with no register in
the function being compiled — at top level and inside functions alike.  Inside a
function such a name may still be shadowed by an enclosing local or by one
created at run time (`locals["x"] = 1`, a `locals` map passed to another
function, `import`), so both opcodes check the frame first and fall back to the
same run-time search `LOADC`/`LOADV` use whenever anything could shadow the
name.  The check costs one integer compare in `@main`, which can never be
shadowed.

`GSTORE` is emitted only by top-level code, since an assignment inside a
function creates a local, not a global.  A hand-written `.msa` may still use it
anywhere; it always writes the global.

| Mnemonic | Description |
| --- | --- |
| GLOADC_rA_iBC | R[A] := the global named by reference BC, invoking it if it is a funcref (globals counterpart of LOADC) |
| GLOADV_rA_iBC | R[A] := the global named by reference BC, as stored (globals counterpart of LOADV; the `@name` form) |
| GSTORE_rA_iBC | the global named by reference BC := R[A] |

Assembly syntax names the global directly, and the assembler interns it:

```
GSTORE r1, "count"     # count = r1
GLOADC r2, "print"     # r2 = print  (invoking it, if it is a function)
GLOADV r2, "print"     # r2 = @print
```

A read whose slot is unassigned — never bound, or removed — falls through to the
intrinsics table, and raises Undefined Identifier if it is not there either.

### Math

| ADD_rA_rB_rC | R[A] := R[B] + R[C] |
| SUB_rA_rB_rC | R[A] := R[B] - R[C] |
| MUL_rA_rB_rC | R[A] := R[B] * R[C] |
| DIV_rA_rB_rC | R[A] := R[B] / R[C] |
| MOD_rA_rB_rC | R[A] := R[B] % R[C] |
| POW_rA_rB_rC | R[A] := R[B] ^ R[C] (exponentiation) |

### Logical (Fuzzy Logic)

| Mnemonic | Description |
| --- | --- |
| AND_rA_rB_rC | R[A] := R[B] and R[C] (fuzzy: AbsClamp01(a * b)) |
| OR_rA_rB_rC | R[A] := R[B] or R[C] (fuzzy: AbsClamp01(a + b - a*b)) |
| NOT_rA_rB | R[A] := not R[B] (fuzzy: 1 - AbsClamp01(b)) |

### Boolean Storage

| Mnemonic | Description |
| --- | --- |
| LT_rA_rB_rC | R[A] := (R[B] < R[C]) |
| LT_rA_rB_iC | R[A] := (R[B] < C (8-bit signed)) |
| LE_rA_rB_rC | R[A] := (R[B] <= R[C]) |
| LE_rA_rB_iC | R[A] := (R[B] <= C (8-bit signed)) |
| EQ_rA_rB_rC | R[A] := (R[B] == R[C]) |
| EQ_rA_rB_iC | R[A] := (R[B] == C (8-bit signed)) |
| NE_rA_rB_rC | R[A] := (R[B] != R[C]) |
| NE_rA_rB_iC | R[A] := (R[B] != C (8-bit signed)) |

### Flow Control

| Mnemonic | Description |
| --- | --- |
| JUMP_iABC | PC += ABC (24-bit signed value) |
| BRTRUE_rA_iBC | if R[A] is true then PC += BC (16-bit signed); raises a runtime error if R[A] is an error |
| BRFALSE_rA_iBC | if R[A] is false then PC += BC (16-bit signed); raises a runtime error if R[A] is an error |
| BRERR_rA_iBC | if R[A] is an error then PC += BC (16-bit signed) |
| NEXT_rA_rB | advance iterator R[A] over collection R[B]; skip the next instruction if there is no next entry.  For a list or string the iterator is a plain index; for a map it is the two-phase encoding described in [MAP_ITERATION.md](MAP_ITERATION.md) |
| ARGBLK_iABC | begin an argument block of ABC `ARG` instructions, which this opcode consumes along with the `CALL` that follows them; see [FUNCTION_CALLS.md](FUNCTION_CALLS.md) |
| ARG_rA | pass R[A] as the next argument.  Only valid inside an `ARGBLK` block, which executes it; reaching one on its own is an internal error |
| ARG_iABC | pass the integer ABC as the next argument.  Same rule as `ARG_rA` |
| CALLF_iA_iBC | call funcs[BC] with parameters/return value at register A |
| CALL_rA_rB_rC | invoke FuncRef in R[C], with stack frame at R[B], result to R[A] |
| RETURN | return with result in R[0] |
| NEW_rA_rB | R[A] := new map with __isa set to R[B] |
| ISA_rA_rB_rC | R[A] := (R[B] isa R[C]) — true if identical or R[C] is in R[B]'s __isa chain |
| METHFIND_rA_rB_rC | R[A] := method lookup on R[B] with key R[C], walking __isa chain; sets pendingSelf=R[B], pendingSuper=containing map's __isa |
| IDXGET_rA_rB_rC | R[A] := R[B][R[C]] with type-map fallback, like METHFIND but never auto-invokes a funcRef result; clears pending context. If R[B] is an error, R[A] := R[B]. Used for both `x[k]` and `@x[k]` |
| SETSELF_rA | Override pendingSelf with R[A] (used for super.method() to preserve original self) |
| CALLIFREF_rA | If R[A] is a funcref and pending context exists, auto-invoke it with pending self/super; otherwise clear pending context |
| ITERGET_rA_rB_rC | R[A] := element at position R[C] from container R[B]; for lists/strings same as INDEX, for maps returns {"key":k, "value":v} |
| ERRCHK_rA | if R[A] is an error, terminate with an "Uncaught" runtime error; otherwise do nothing |

(More opcodes will be added as the prototype develops.)

### Naming a variable past constant 255

The `kC` operand is only 8 bits, so `LOADV_rA_rB_kC` and `LOADC_rA_rB_kC` can
only name one of the first 256 constants.  A function whose constant pool grows
beyond that (a big module body compiled as `@main` gets there easily) needs the
`rC` forms, which take the expected name from a register instead:

```
LOAD  r7, k300      # LOAD_rA_kBC — 16-bit constant index
LOADC r4, r0, r7    # name comes from r7, not the constant pool
```

`CodeGenerator.EmitNamedLoad` picks the form automatically, so the two-
instruction sequence appears only where it is actually needed.  Note that the
register operands are still 8-bit, so this raises the constant ceiling to 65535
but leaves the 256-register-per-function limit unchanged.

## Assembly Language

While an official assembly language (or assembler) will probably not be part of MiniScript 2.0, it will be darned handy during development.  So let's define one we can live with for the next year.  To be somewhat easier to read and write than the above opcodes, we'll remove the operand usage codes, and instead use prefixes on the actual operands: "r" for register, "k" for constant, no prefix for a signed integer.  We'll also allow some degree of constant and label lookup by the assembler itself.  Examples:

| Assembly Code | Opcode and Operands |
| --- | --- |
| LOAD r2, r5 | LOAD_rA_rB 2, 5, 0 |
| LOAD r6, 42 | LOAD_rA_iBC 6, 42 |
| LOAD r3, k20 | LOAD_rA_kBC 3, 20 |
| LOAD r12, "foo" | LOAD_rA_kBC 12, 7 (if k[7] == "foo") |
| ASSIGN r1, r2, "x" | ASSIGN_rA_rB_kC 1, 2, 3 (if k[3] == "x") |
| NAME r0, "result" | NAME_rA_kBC 0, 5 (if k[5] == "result") |
| CHKNAME r0, "result" | CHKNAME_rA_kBC 0, 5 (if k[5] == "result") |
| ADD r5, r3, r4 | ADD_rA_rB_rC 5, 3, 4 |
| ADD r5, r3, 42 | *error* (unless we add an ADD_rA_rB_iC opcode!) |

This assembly representation is easier to read and write, but it does require the writer to remember which opcode modes are available, as the last example shows.

## Comparison and Branching

Each comparison operator (LT: less than, LE: less than or equal, EQ: equal, NE: not equal) has two opcodes, which work just like the math opcodes:

  - `LT_rA_rB_rC` computes R[B] < R[C], and stores the result in R[A]
  - `LT_rA_rB_iC` does the same with an 8-bit signed immediate as the right operand

A comparison therefore always lands in a register, and a conditional jump is a
separate instruction that tests that register.  Earlier designs also had a
compare-and-branch family (`BRLT` and friends) and a conditional-skip family
(`IFLT` and friends), on the theory that a later optimization pass would fuse a
comparison into the branch that consumes it.  That pass was never written, the
code generator never emitted any of those 22 opcodes, and they were removed
rather than left as dispatch-table weight.  Measurements before removing them
put the fusion at roughly 5-10% on a tight `while` loop and nothing at all
elsewhere -- `for` loops compile to `NEXT`/`ITERGET` and never compare-and-branch
at all.  Should that trade look better later, the place to start is the loop-
invariant `LOAD`s of constants, which cost considerably more on the same loops.

For when we have a truth value already in a register (a common situation when compiling `if` statements), there are also:

- `BRTRUE_rA_iBC` jumps ±32767 steps if the register value is truthy
- `BRFALSE_rA_iBC` jumps ±32767 steps if the register value is falsey
- `BRERR_rA_iBC` jumps ±32767 steps if the register value is an error

`BRTRUE`/`BRFALSE` deliberately raise a runtime error when given an error value, since an error used as an `if`/`while` condition should terminate the program.  `BRERR` makes no such judgment — it simply tests for an error value without raising — which is what lets short-circuit `and`/`or` handle errors (see below).

### Discarded errors: `ERRCHK`

Error values propagate lazily: they flow through assignments, arguments, and most operators as ordinary values, and only terminate the program when used somewhere that cannot tolerate them.  That leaves one hole — an expression compiled as a *bare statement* throws its value away, so an error there would simply vanish (`file.readLines("/nope")` or `import "broken"` on a line by itself would do nothing at all).

So the code generator follows every bare-expression statement with `ERRCHK` on the register holding its result.  An error nobody stored and nobody passed on is an error nobody can catch, and `ERRCHK` halts there, reporting `Uncaught <message>` with the discarding line in the stack trace.  Statements proper (assignment, `if`, `while`, `for`, `break`, `continue`, `return`) have no meaningful result register and get no check; `ASTNode.IsStatement()` is what tells the two apart.

This is also what makes an error a *catchable* result: `import "broken"` terminates, while `e = import("broken")` stores the error for the caller to inspect.  An intrinsic that wants a failure to be catchable should therefore return an error value rather than raising a runtime error itself.

### Short-circuit `and`/`or`

The `and` and `or` operators short-circuit: the right operand is not evaluated when the left operand alone determines the result.  Because an error value must *not* short-circuit through `BRTRUE`/`BRFALSE` (that would raise), the code generator emits a `BRERR` first to peel off the error case, after which the surviving `BRTRUE`/`BRFALSE` only ever sees a non-error value:

- `a and b`: if `a` is an error, the result is that error (right operand skipped); if `a` is false, the result is `0` (right operand skipped); otherwise `b` is evaluated and combined with `AND_rA_rB_rC`.
- `a or b`: if `a` is an error, `b` is evaluated and combined with `OR_rA_rB_rC` (so `someErr or fallback` yields `fallback`); if `a` is *fully* true, the result is `1` (right operand skipped); otherwise `b` is evaluated and combined with `OR_rA_rB_rC`.

A subtlety for `or`: short-circuiting to `1` is only valid when the left operand alone forces the fuzzy result to `1` — that is, when its fuzzy value is >= 1.  A *partial* truth value such as `0.5` must **not** short-circuit, since `0.5 or x` is genuinely fuzzy.  The code generator tests this by negating: `not a` is false exactly when `a` is fully true, which reduces the question to a plain `BRFALSE`.  (The `and` side needs no such trick: a fuzzy `and` is `0` exactly when the left operand is false, which `BRFALSE` already tests directly.)

A conditional jump's target is usually not known when the jump is emitted, so the code generator emits it with a placeholder offset and "back-patches" it once the target is reached.  `BRTRUE`/`BRFALSE` carry a 16-bit signed offset, which is wide enough that the back-patch never has to reconsider the instruction it chose:

```
  LT_rA_rB_rC 3, 5, 4    # r3 = (r5 < r4)
  BRFALSE_rA_iBC 3, 0    # if not, jump past the body (target TBD)
  ...
  # target found: rewrite that 0 as the real offset
```

Note that all jump/branch targets are relative to the *next* instruction.  So, `JUMP_iABC 0` would do the same as `NOOP`, and `JUMP_iABC -1` would put the machine into a tight infinite loop.

## Function Calls

(To-Do.)
