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
| LOADV_rA_rB_kC | R[A] := R[B], but verify that register B has name matching constants[C] |
| LOADC_rA_rB_kC | R[A] := R[B], but verify name matches constants[C] and call if funcref |
| FUNCREF_iA_iBC | R[A] := make_funcref(BC) (create function reference to function BC) |
| ASSIGN_rA_rB_kC | R[A] := R[B] and name[A] := constants[C] (copy value and assign variable name) |
| NAME_rA_kBC | name[A] := constants[BC] (assign variable name without changing value) |
| LIST_rA_iBC | R[A] := new list with capacity BC |
| MAP_rA_iBC | R[A] := new map with initial capacity BC |
| PUSH_rA_rB | push R[B] onto list R[A] |
| INDEX_rA_rB_rC | R[A] := R[B][R[C]] (get element R[C] from list R[B]) |
| IDXSET_rA_rB_rC | R[A][R[B]] := R[C] (set element R[B] of list R[A] to R[C]) |
| LOCALS_rA | R[A] := new VarMap for local variables (r0-r4) |

### Math

| ADD_rA_rB_rC | R[A] := R[B] + R[C] |
| SUB_rA_rB_rC | R[A] := R[B] - R[C] |
| MULT_rA_rB_rC | R[A] := R[B] * R[C] |
| DIV_rA_rB_rC | R[A] := R[B] / R[C] |
| MOD_rA_rB_rC | R[A] := R[B] % R[C] |

### Boolean Storage

| Mnemonic | Description |
| --- | --- |
| LT_rA_rB_rC | R[A] := (R[B] < R[C]) |
| LT_rA_rB_iC | R[A] := (R[B] < C (8-bit signed)) |
| LT_rA_iB_rC | R[A] := (B (8-bit signed) < R[C]) |
| LE_rA_rB_rC | R[A] := (R[B] <= R[C]) |
| LE_rA_rB_iC | R[A] := (R[B] <= C (8-bit signed)) |
| LE_rA_iB_rC | R[A] := (B (8-bit signed) <= R[C]) |
| EQ_rA_rB_rC | R[A] := (R[B] == R[C]) |
| EQ_rA_rB_iC | R[A] := (R[B] == C (8-bit signed)) |
| NE_rA_rB_rC | R[A] := (R[B] != R[C]) |
| NE_rA_rB_iC | R[A] := (R[B] != C (8-bit signed)) |

### Flow Control

| Mnemonic | Description |
| --- | --- |
| JUMP_iABC | PC += ABC (24-bit signed value) |
| BRTRUE_rA_iBC | if R[A] is true then PC += BC (16-bit signed) |
| BRFALSE_rA_iBC | if R[A] is false then PC += BC (16-bit signed) |
| BRLT_rA_rB_iC | if R[A] < R[B] then PC += C (8-bit signed) |
| BRLT_rA_iB_iC | if R[A] < B then PC += C (8-bit signed) |
| BRLT_iA_rB_iC | if A < R[B] then PC += C (8-bit signed) |
| BRLE_rA_rB_iC | if R[A] <= R[B] then PC += C (8-bit signed) |
| BRLE_rA_iB_iC | if R[A] <= B then PC += C (8-bit signed) |
| BRLE_iA_rB_iC | if A <= R[B] then PC += C (8-bit signed) |
| IFLT_rA_rB | if R[A] < R[B] is **false** then PC += 1 |
| IFLT_rA_iBC | if R[A] < BC is **false** then PC += 1 |
| IFLT_iAB_rC | if AB < R[C] is **false** then PC += 1 |
| IFLE_rA_rB | if R[A] <= R[B] is **false** then PC += 1 |
| IFLE_rA_iBC | if R[A] <= BC is **false** then PC += 1 |
| IFLE_iAB_rC | if AB <= R[C] is **false** then PC += 1 |
| IFEQ_rA_rB | if R[A] == R[B] is **false** then PC += 1 |
| IFEQ_rA_iBC | if R[A] == BC is **false** then PC += 1 |
| IFNE_rA_rB | if R[A] != R[B] is **false** then PC += 1 |
| IFNE_rA_iBC | if R[A] != BC is **false** then PC += 1 |
| CALLF_iA_iBC | call funcs[BC] with parameters/return value at register A |
| CALLFN_iA_kBC | call function named constants[BC] with params/return at rA |
| CALL_rA_rB_rC | invoke FuncRef in R[C], with stack frame at R[B], result to R[A] |
| RETURN | return with result in R[0]

(More opcodes will be added as the prototype develops.)

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
| ADD r5, r3, r4 | ADD_rA_rB_rC 5, 3, 4 |
| ADD r5, r3, 42 | *error* (unless we add an ADD_rA_rB_iC opcode!) |

This assembly representation is easier to read and write, but it does require the writer to remember which opcode modes are available, as the last example shows.

## Comparison and Branching

For each comparison operator (LT: less than, LE: less than or equal, EQ: equal, NE: not equal), there are several related opcodes:

- `BR` (branch) opcodes take a short (±127) jump if the comparison is true.
  - `BRLT_rA_rB_iC` jumps by C (8-bit signed) if R[A] < r[B]
  - `BRLT_rA_iB_iC` jumps by C (8-bit signed) if R[A] < B (8-bit signed)
- `IF` opcodes execute the next instruction if the comparison is true; otherwise, they skip over it.
  - `IFLT_rA_rB` skips the next instruction unless R[A] < r[B]
  - `IFLT_rA_iBC` skips the next instruction unless R[A] < BC (16-bit signed)
- plain comparison opcodes work just like the math opcodes
  - `LT_rA_rB_rC` computes R[B] < R[C], and stores the result in R[A]

For when we have a truth value already in a register (a common situation when compiling `if` statements), there are also:

- `BRTRUE_rA_iBC` jumps ±32767 steps if the register value is truthy
- `BRFALSE_rA_iBC` jumps ±32767 steps if the register value is falsey

Note that when emitting code, we often don't know whether a branch is going to be ±127 steps or less; we "back-patch" the jump later, once we've found the branch target.  We can accomplish that with these opcodes by first emitting a long branch:

```
  IFLT_rA_rB 5, 3  # if r5 < r3 then
  JUMP_iABC 0      # jump (target TBD)
```

and then back-patching it as needed:

```
  IFLT_rA_rB 5, 3  # if r5 < r3 then
  JUMP_iABC 42     # jump to end of loop
```

Then, in an optimization phase, we can easily spot `IF`-`JUMP` pairs where the jump offset is small enough to replace the pair with a BRanch:

```
  BRLT_rA_rB_iC 5, 3, 42  # if r5 < r3 then goto end of loop
  NOOP
```

And the NOOP could then be optimized away, making the code shorter.  (All jumps/branches that cross this point would have to be updated, but as that only makes the branch targets shorter, this can always be done.)

Note that all jump/branch targets are relative to the *next* instruction.  So, `JUMP_iABC 0` would do the same as `NOOP`, and `JUMP_iABC -1` would put the machine into a tight infinite loop.

## Function Calls

(To-Do.)
