# Globals

A rethink of how MiniScript 2 represents the global namespace.

The current (July 2026) implementation grew a piece at a time — REPL globals, then chaining
to a new program, then a host Get/Set API, then a second interpreter — and each
piece was fitted around the one before it.  The result works in most of the
cases it was tested against and is confusing in all of them.  This document
argues that the trouble comes from one wrong equation at the bottom, proposes a
different one, and works through every case we know of against it.

---

## 1. What we do today

There are **three** representations of "the globals" in the tree right now, and
which one is authoritative depends on how the interpreter was started.

| # | Representation | Lives in | Used when |
|---|---|---|---|
| 1 | `@main`'s register window, `stack[0 .. @main.MaxRegs)`, with names in `names[]` | `VM.stack` | always, for any global `@main` statically mentions |
| 2 | `callStack[0].LocalVarMap` — a `VarMap` over that window, plus a hash table for everything else | `VM.callStack[0]` | non-REPL, created lazily by `GetGlobalsVarMap()` |
| 3 | `Interpreter._replGlobals` / `VM.ReplGlobals` — a persistent `VarMap`, rebound onto a new register window every line | `Interpreter` + `VM` | REPL, and `ResetPreservingGlobals` |

A `VarMap` (`cs/VarMap.cs`) is a `GCMap` with a `VarMapBacking`: a set of
name → register-index bindings layered *over* an ordinary hash table.  A key
resolves to a register if it happens to be bound, and to a hash entry
otherwise.  Which one it is, for a global, is decided by whether the currently
compiled `@main` mentioned that name.

Everything awkward in the current code follows from keeping those three in
agreement:

- **`VM.Reset(functions, replGlobals)`** allocates a brand-new 10240-entry
  `stack` and `names` pair *on every REPL line*, then calls `Rebind`, which
  `Gather`s the old register-backed entries into the hash table and re-attaches
  the backing to the new arrays (`cs/VM.cs:655`).
- **`ASSIGN_rA_rB_kC` and `NAME_rA_kBC`** carry a
  `baseIndex == 0 && !ReplGlobals.IsNull()` test and a `MapToRegister` call
  (`cs/VM.cs:1326`, `:1348`) — on the interpreter's hot path — whose job is to
  migrate a value *back out* of the hash table into the register that the new
  `@main` just decided to use for it.
- **`MapToRegister`** has to detach `_vmb` around its own `TryGet`+`Remove`
  (`cs/VarMap.cs:196`) so it doesn't read the register it is in the middle of
  binding.
- **`Value.Rebind`** has to detect a previously-`Gather`ed map and re-attach an
  empty backing (`cs/Value.cs:1041`), because otherwise the map silently freezes
  at the instant of the `Gather` while the VM carries on using registers.
- **`Interpreter.SetGlobalValue`** writes to `_replGlobals` and therefore does
  **nothing at all** outside REPL mode (`cs/Interpreter.cs:594`), while
  `GetGlobalValue` reads `vm.GetGlobalsVarMap()` and answers in both modes.
  Set and Get disagree.
- **`REPL("")`** exists as a bootstrap whose only purpose is to build
  representation 3 so that `SetGlobalValue` has somewhere to write
  (`cs/Interpreter.cs:385`).
- **`Interpreter.Reset()`** clears `_keptGlobals` but not `_replGlobals`, so
  `Reset` followed by `REPL` resurrects the previous program's globals.
- **`RETURN` from `@main`** `Gather`s `callStack[0].LocalVarMap` and sets it to
  null (`cs/VM.cs:2471`).  In non-REPL mode that map *is* the globals; after the
  program ends, the next `GetGlobalsVarMap()` builds a fresh `VarMap` over the
  registers and the gathered hash entries — every global the program created
  dynamically — are gone.  A host reading globals after a run gets a partial
  answer, and which part it gets depends on what `@main` happened to name.
- **`ResetPreservingGlobals`** is a fourth path threaded through
  `_keptGlobals`, existing only because representations 2 and 3 don't meet.
  `raylib-miniscript/notes/HOSTING_MS.md` records that a shell crossing between
  them constantly is what forced `Interp.load` to route programs through `REPL`.

That is a lot of machinery, and it is all in service of one thing.

## 2. The wrong equation

> **globals = `@main`'s locals = a register window at `stack[0]`**

Each `=` is false.

**Lifetime.**  A register window belongs to one compilation of `@main`.  The
global namespace outlives compilations: every REPL line is a new `@main`, `run`
chains to a new program, an embedded interpreter loads program after program.
So the namespace has to be lifted off the window and carried across — hence
`Gather`, `Rebind`, and the stack reallocation.

**Extent.**  A register window's size is fixed at compile time.  The set of
globals is open-ended at *run* time: `globals.foo = 42` from ten frames down,
`globals[name] = v` with a computed name, `SetGlobalValue` from the host, the
next REPL line.  None of those can get a register, so they get a hash entry
instead — hence the two-tier `VarMap`.

**Identity.**  Because of the above, *where a global lives* depends on whether
the current `@main` mentioned its name.  That is an artifact of compilation, and
it leaks into semantics: it is why Get and Set disagree, why `Gather` loses
half the namespace, and why `MapToRegister` has to shuffle values between tiers
as new bytecode declares new names.

The user-visible rule we actually want is the one the brief states:

> `globals.foo = 42` deep in a call stack must mean exactly what `foo = 42`
> typed at the next REPL prompt means.

Today those are two different storage operations that we work to keep
consistent.  They should be *the same operation*.

## 3. The right equation

> **The global namespace is a first-class object with its own storage.  It is
> not a call frame, and it does not belong to any compiled program.**

Concretely: one heap object, `Globals`, holding a growable table of *slots*.
Every global — named by compiled code, created by `globals.x =`, seeded by the
host, typed at the REPL — is a slot in that table.  There is exactly one place
a global can be, so there is nothing to keep in sync.

`@main` keeps a register window, but it holds only *temporaries*.  Top-level
named variables are not registers at all.  This is the load-bearing decision:
it is what removes the dual storage, and everything else follows from it.

### 3.1 The `Globals` object

```csharp
public class Globals {
	private List<Value> _names;             // slot -> name (never cleared)
	private List<Value> _values;            // slot -> value, or Value.Unassigned
	private Dictionary<Value, Int32> _index; // name -> slot
	public Int32 Id;                        // generation, for cache validation
}
```

Properties that matter:

- **Slots are stable.**  Once `"foo"` is slot 37 it is slot 37 for the life of
  the object.  Removing `foo` sets `_values[37] = Unassigned` and *keeps the
  slot*; re-adding `foo` reuses it.  Nothing that caches a slot index ever goes
  stale.
- **The table grows.**  A new name appends.  No ceiling, no `MaxRegs`, no
  reallocation of anything the VM points into.
- **Unassigned is a real state.**  It distinguishes "slot exists but the name is
  not bound" from "bound to null" — exactly the distinction `names[i].IsNull()`
  encodes for registers today.  It is represented by a single sentinel `Value`
  (see below), not by a parallel `List<Boolean>`, so testing it is one compare.
- **Keys need not be strings.**  `_index` is keyed on `Value`, so
  `globals[42] = 1` works as it does today, and `globals` stays a fully general
  map.

Slot exhaustion is not a practical concern: only *distinct names ever used* consume
slots, and remove/re-add reuses.

#### The `Unassigned` sentinel

A new NaN-box tag would do (0xFFFA–0xFFFD are free, `cs/Value.cs:54`), but there
is a cheaper way that touches nothing: **allocate one unique funcref at startup
and use it as the sentinel.**

```csharp
public static Value Unassigned;   // set up alongside GCManager.Init()
```

This is safe because funcrefs compare by *identity*.  `ScalarEqual`
(`cs/Value.cs:643`) tries `RefEquals` and then falls straight through to
`return false` for two distinct same-type reference values, so no value a user
can construct is equal to it under either `==` or `RefEquals` — and the sentinel
is never handed out, so no user value can be it.  `GCManager.AddRoot`
(`cs/GCManager.cs:162`) keeps it alive, the same way intrinsic funcrefs are kept
alive.  `GCManager.Init()` is called exactly once (`cs/App.cs:66`), so the
sentinel is stable for the life of the process; it should be created there, so
that a host which re-inits gets a matching one.

The test costs the same as a tag test — `RefEquals` is a bare `_u == rhs._u`
(`cs/Value.cs:613`), so one static load and a compare against a mask-and-compare
— and it leaves the NaN-box layout, its C++ header constants, and every `Is*()`
classifier untouched.  That is a real reduction in blast radius over adding a
tag.

The one hazard is that a leaked sentinel is *more* dangerous than a leaked tag
would be: `IsFuncRef()` returns true for it, so `LOADC` would auto-invoke it.
That is also the cure.  Give the sentinel a `FuncDef` whose `NativeCallback`
raises an internal error immediately.  An escape then becomes a loud, located
diagnostic rather than a silent wrong value — strictly better than what a tag
would have given us, where an escaped `Unassigned` would just be an inert oddity
propagating quietly.

### 3.2 `globals` the MiniScript value

`globals` is a `GCMap` whose *entire* storage is the `Globals` table, with
`Items` left permanently null.  `Globals` serves as its own map backing, in the
slot `VarMapBacking` occupies for a frame; there is no separate
`GlobalsBacking` class, since the two would have been one to one.
`TryGet`/`Set`/`Remove`/`NextEntry`/`KeyAt`/`ValueAt`/`MarkChildren` all
delegate to the table.

Two consequences worth stating:

- There is **no** hash-vs-register split inside the globals map, so
  `globals.foo = 42` and top-level `foo = 42` reach the same slot by
  construction rather than by synchronization.  That is the property the brief
  asks for, and it is now a structural fact rather than something to maintain.
- Iteration over `globals` becomes O(1) per step (walk the slot array, skipping
  `Unassigned`).  Today `GCMap.KeyAt(i)` walks the `Dictionary` from the start
  (`cs/GCItems.cs:256`), making iteration quadratic.

`VarMapBacking` stays, unchanged in purpose, for real call frames.  Everything
hard about it — `Rebind`, the `Gather`-then-re-import dance, `MapToRegister`'s
detach — was driven by the globals case and can go (see §6).

### 3.3 Who owns it

`Interpreter` owns a `Globals`, created in its constructor, so it exists before
any compile.  `VM` holds a reference (`vm.Globals`), assigned at `Reset`; a
standalone VM with no interpreter creates its own.

The reference is on the **VM**, deliberately.  `HOSTING_MS.md` depends on a
function compiled in interpreter A, called from VM B, resolving its globals *in
B* — that is what makes seeding a child interpreter with the parent's library
layer work.  With globals reached through `vm.Globals`, that stops being an
accident of `LookupVariable`'s fall-through order and becomes the stated rule.

Two interpreters can also *share* one `Globals` object if a host wants that.
Nothing prevents it, and nothing else has to know.

## 4. Access paths

### 4.1 From the host

```csharp
public Value GetGlobalValue(String name);
public void  SetGlobalValue(String name, Value v);
```

Both go straight to the table.  They work before the first compile, after the
program ends, at any call depth, in REPL mode and out of it, and they are
symmetric because they are the same slot.  The `REPL("")` bootstrap is no longer
needed for anything.

### 4.2 From MiniScript, via the map

`globals.x`, `globals["x"]`, `globals.remove`, `globals.indexes`, `globals.len`
— all ordinary map operations on the backing.  Nothing special.

### 4.3 From compiled code

This is the only part with a performance question, and it has two answers.

**Model-only (recommended first step).**  Top-level named variables compile to
ordinary map get/set against the globals map.  No new opcodes.  Every case in
§5 already works at this point, and the entire mess in §1 is already gone.  Cost:
a hash lookup per top-level variable access, where today (outside the REPL) it
is a register move.

**Slot-indexed (the optimization).**  Two new opcodes and a small per-`FuncDef`
table:

```
GETGLOBAL_rA_iBC     R[A] = globals[slot of ref BC]     (auto-invokes funcrefs, like LOADC)
GETGLOBALV_rA_iBC    same, no auto-invoke               (like LOADV, for @x)
SETGLOBAL_iBC_rA     globals[slot of ref BC] = R[A]
```

`BC` indexes a per-`FuncDef` global-reference table: `List<Value> GlobalNames`
filled at compile time, `List<Int32> GlobalSlots` filled at run time, plus a
single `Int32 GlobalCacheId`.  The access is:

```csharp
if (curFunc.GlobalCacheId != globals.Id) ResolveGlobalRefs(curFunc, globals);
Value v = globals.ValueAtSlot(curFunc.GlobalSlots[bc]);
if (v.IsUnassigned()) v = GlobalMiss(curFunc.GlobalNames[bc]);  // intrinsics, else error
```

One integer compare, then two array indexes.  Compare that to today's
`LOADV_rA_rB_kC`, which loads `constants[c]`, loads `names[base+b]`, compares
them, branches, and copies — the same shape and roughly the same cost.  For
REPL top-level code it is strictly *faster* than today, which pays a linear
`MapToRegister` scan on every `ASSIGN`/`NAME`.

Notes on this path:

- The guard is per-`FuncDef`, not per-reference, so one compare validates the
  whole table.  It can be hoisted into `SwitchFrame` later if measurement wants
  it; checking per access is more robust (no choke-point discipline needed for
  `RunFunction`, `ManuallyPushCall`, re-entrancy).
- `Id` changes only when the name → slot mapping could change meaning: a new
  `Globals`, or `reset`.  Adding a slot never invalidates an existing one, so
  the cache essentially never misses.
- `ResolveGlobalRefs` *creates* a slot for every name the function mentions,
  including ones it never reaches.  Harmless: they stay `Unassigned`, read as
  Undefined Identifier, and are skipped by iteration.
- Intrinsics stay out of the table, so `globals.indexes` does not suddenly list
  150 built-in names.  A read that finds `Unassigned` falls through to the
  intrinsics table — one predictable, never-taken branch on the hot path, and
  the same lookup cost as today's `LookupVariable` for intrinsic calls.  User
  shadowing of `print` keeps working, which `HOSTING_MS.md` relies on.
- Only **global scope** compiles to these opcodes.  Inside a function a free
  name might be an enclosing local, so those keep emitting `LOADC`/`LOADV` and
  resolving through `LookupVariable`, whose globals step is now a table lookup.
  Teaching the code generator to prove a name is free through the whole lexical
  chain, and emit `GETGLOBAL` for it, is worthwhile follow-on work — it would
  make `print` inside a function fast too — but it is not part of this change.
- Compiler-internal temporaries at top level (loop list/index registers, and so
  on — `cs/CodeGenerator.cs:1154`) stay registers.  Only user-named variables
  become slots.

**Recommendation: land the model, measure, then decide on the slots.**  The
object model is the fix; slot indexing is a performance detail that can be added
underneath it without disturbing any of §5.  Splitting them this way also keeps
the risky part (new opcodes, assembler, disassembler, `.msa` compatibility)
separate from the part that has to be right.

## 5. Every case, worked through

**`Interpreter.Reset(source)`** — `Globals = new Globals()`.  One line, and it
replaces the `_replGlobals`-not-cleared bug, `_keptGlobals`, and the
representation-2-vs-3 question.

**`Interpreter.ResetPreservingGlobals(source)`** — `Reset` without that line.
No `GetGlobalsVarMap`, no `Rebind`, no fresh stack arrays, no `_keptGlobals`
handoff.  Functions carried over from the old program keep resolving their
globals, because they resolve against `vm.Globals`, which did not change.

**`Interpreter.Get/SetGlobalValue`** — §4.1.  Symmetric, mode-independent,
available before the first compile and after the last statement.

**A REPL** — each line compiles a fresh `@main` whose top-level assignments
write slots in the same table.  No persistent-globals parameter to `VM.Reset`,
no stack reallocation, no rebinding, no `MapToRegister` on the hot path, no
`ReplGlobals` field.  `REPL` and `Reset`+`Compile` are now the *same* globals
regime, which is precisely what `HOSTING_MS.md` reports the Mini Micro shell
needs and could not get.

**Code doing `globals.foo = 42` mid-run** — writes slot `foo`, creating it if
new.  Immediately visible to: code already running (it reads the slot), the
`globals` map (same slot), the host (same slot), and the next REPL line (same
slot).  Identical to `foo = 42` at top level, by construction.

**`globals[name] = v` with a computed name** — same, via the map backing.

**`globals.remove "x"`** — slot marked `Unassigned`; reads raise Undefined
Identifier; iteration skips it; a later `x = 1` reuses the slot.

**The `reset` intrinsic, mid-statement** — *implemented as `Globals.Clear()`
rather than as the replacement this section originally proposed.*  Clearing in
place keeps the object, its map, its slot numbering, and its `Id`, so nothing
anywhere can be left holding a stale namespace — including the globals map that
the running `@main` has cached in a register.  Because there are no register
copies of individual globals, the clear is genuinely immediate, for the rest of
the line that called it included.  (The old code cleared `ReplGlobals` while the
running `@main` kept answering from its registers.)

**`locals` at top level** — returns the globals map, which is correct
MiniScript semantics and is now literally the same object rather than a
`VarMap` that aliases it.  `callStack[0].LocalVarMap` is unused and goes away,
which also removes the `RETURN`-from-`@main` `Gather` that currently discards
half the namespace.

**`outer` at top level** — the globals map.  A function defined at top level
captures it as `OuterVarMap`; since the object is now stable for the life of the
namespace, a function defined on REPL line 3 still resolves correctly on line
23 with nothing rebound.

**`import`** — unchanged.  A module body compiles as a *function* returning its
`locals` (`CodeGenerator.CompileImport`), so module top-level names are locals,
not globals.  A module that writes `globals.zzz = ...` hits the shared table.
`VM.SetVar`, which `import` uses to bind the module map in the calling frame
(`cs/ShellIntrinsics.cs:2356`), loses its `callStackTop <= 1` special case: at
depth 0 the "current frame's locals map" simply *is* the globals map.

**Hosting an interpreter within MiniScript** — each `Interpreter` gets its own
`Globals` with its own `Id`.  Seeding is a slot-by-slot copy that never mutates
the source, which is what `HOSTING_MS.md` asks for ("enumerate the source
*without* calling `Gather`") — and here there is nothing to gather.  A
parent-compiled function called from the child resolves in the child's table via
the id-guarded cache.  Nesting works for the same reason.  Sharing one `Globals`
between parent and child is available if a host wants it.

**GC** — one root: the globals map Value on the VM.  `Globals.MarkChildren`
marks names and values.  This replaces marking `ReplGlobals`, marking
`callStack[0].LocalVarMap`, and the comment at `cs/VM.cs:363` explaining that
gathered entries are reachable only from that mark.

**Frozen globals** — `globals.Freeze()` must make slot writes fail; the check
goes in the backing alongside the existing `GCMap.Frozen` handling.

## 6. What gets deleted

All done in stage 3 except where noted.

- `VM.ReplGlobals` and both hot-path tests in `ASSIGN_rA_rB_kC` / `NAME_rA_kBC`
- `VM.Reset(allFunctions, replGlobals)`'s entire partial-reset branch, including
  the per-line reallocation of `stack` and `names`.  The overload now takes a
  `Globals` instead, and null means "keep the one this VM already has".
- `Value.Rebind` and `VarMapBacking.Rebind` (globals were the only caller), in
  both the C# and the hand-written C++ (`cpp/core/value_map.cpp`, `value.h`)
- ~~`VarMapBacking.MapToRegister`'s detach-around-lookup~~ — **kept.**  Real
  call frames still need it for `locals["x"] = 1` before `x` is named, which is
  exactly the case the detach exists for.
- `Interpreter._replGlobals`, `_keptGlobals`, and `ResetReplGlobals` (the last
  replaced by `ClearGlobals`)
- the `REPL("")` bootstrap special case
- `GetGlobalsVarMap`'s two-branch logic.  The name is kept, since embedding
  hosts call it; it now just returns `Globals.AsMap()`.
- `callStack[0].LocalVarMap` and the `Gather` of it on `RETURN` from `@main`.
  Both are now dead by construction — nothing ever sets that field, because
  `GetCurrentLocalVarMap` returns the globals map at depth 0 — so the `RETURN`
  code is left as-is and simply never fires there.
- `VM.SetVar`'s `callStackTop <= 1` branch

One thing this list did not anticipate: `VM.FindShortName` scanned `@main`'s
named registers, so it had to be rewritten to walk the slot table.  That is what
changes the one test expectation noted in §8.

## 7. Risks and open questions

**Top-level performance.**  *Measured; see the stage 2+3 section of
[GLOBALS_BASELINE.md](GLOBALS_BASELINE.md).*  The regression landed at 4.23x on
`Global Loop`, worse than the 2.5x this section expected, and `Global Churn`
improved 2.8x.  The answer to "are the slot opcodes needed?" is yes.  `.msa`
benchmarks are unaffected, as predicted — they address registers directly and
never touch the globals namespace.

Three benchmarks now cover this — `global_loop`, `global_loop_fn`, and
`global_churn` in `tools/benchmarks/` — and the pre-change numbers are recorded
in [GLOBALS_BASELINE.md](GLOBALS_BASELINE.md).  Two findings from that baseline
bear directly on the design:

- MS2 pays **no** globals penalty today (1.0x globals-vs-locals on the same
  loop), where Python pays 2.5x and Lua 3.4x.  Those two bracket where the
  model-only step lands, and they are the bar the slot step has to beat.
- Reaching globals by name from inside a function is **33x slower than the
  identical loop on an ordinary map**, because of `VarMapBacking`'s linear
  scans.  The redesign should mostly erase that, which may well outweigh the
  top-level regression on real code.

**C++ pointer invalidation.**  The slot array grows.  Any raw base pointer into
it must be re-fetched after anything that can add a slot (a `SETGLOBAL` miss, or
any call).  Simplest is not to cache a base pointer at all — one extra
indirection.  Worth confirming against how `stackPtr`/`localStack` are handled
in the generated dispatch loop.

**The `Unassigned` sentinel must not leak.**  Every read path has to check it.
The audit is bounded — slot reads happen in exactly three places: the opcode,
the backing's `TryGet`, and `LookupVariable` — and with the raising
`NativeCallback` described in §3.1, anything missed reports itself the first
time it is touched rather than corrupting a result.  This is the main thing to
get right in the stage-1 unit tests.

**~~Transpiler: a common abstract base for the two backings.~~**  *Settled in
stage 1: don't.*  `cs/GCInterfaces.cs:21-28` records a deliberate project rule
that GC-managed types here avoid vtables — a vtable pointer written by a static
constructor can be zero in BSS on some platforms, segfaulting on the first
virtual call, which is why `IGCSet` was denied interface status.  An abstract
`MapBacking` held by value inside `GCMap` is that same pattern.  So `GCMap`
carries two mutually exclusive nullable fields, `_vmb` and `_gb`.  A null check
is also cheaper than virtual dispatch, and neither is a hot path.

**Bytecode surface (slot step only).**  New opcodes mean assembler and
disassembler support, and a way to write a global reference in `.msa`
(`GETGLOBAL r1, "name"`, with the assembler interning into the FuncDef's table).
Existing `.msa` files use `ASSIGN`/`NAME` on registers and `CALLF` on explicit
function references, so they keep working — and keep register speed — but their
top-level names stop being visible as globals to called functions.  Nothing in
`examples/` relies on that; worth a sweep to confirm.

**Host API.**  `GetGlobalsVarMap()` stays, returning the globals map, so
embedding hosts keep compiling.

## 8. Staging

1. ~~`Globals` + `GlobalsBacking`, with unit tests standing alone.~~  **Done.**
   `cs/Globals.cs`, `GCMap._gb`, `GCManager.Unassigned`, `Value.IsUnassigned()`,
   and `UnitTests.TestGlobals`.  Nothing is wired into the VM yet, so behavior
   is unchanged and 695/695 integration tests still pass.
2. ~~Wire `vm.Globals` and `Interpreter.Globals` while `@main` stays
   register-based, so both representations exist for one commit.~~
   **Abandoned: not implementable.**  Once a top-level variable exists,
   `x = x + 1` compiles straight into its register (see `Visit(AssignmentNode)`
   in `cs/CodeGenerator.cs`) — no `ASSIGN`, no `NAME`, no opcode at all.  There
   is therefore no hook to write through to the slot table and none to read back
   from it, so `globals.x` would go stale the moment any arithmetic updated `x`.
   The current code only avoids this because the globals `VarMap` reads the
   register *live*.  Merged into stage 3.
3. **Done.**  Wiring and code generator in one commit: `Globals` is
   authoritative, global scope compiles named variables to map get/set on the
   globals map, and everything in §6 is deleted.  701/701 integration tests.
   One expected-output change, in the "first assignment reads the enclosing
   variable" test: a nested collection reached via `globals.m = ...` now
   abbreviates to `m` when printed, exactly as one reached via a plain top-level
   `m = ...` always did.  The two agreeing is the point of the change.
4. **Required, not optional** — see the measurements appended to
   [GLOBALS_BASELINE.md](GLOBALS_BASELINE.md).  The model-only step costs 4.23x
   on top-level access, overshooting the ~2.5x §4.3 predicted, while winning
   2.8x on `Global Churn`.  So: `GETGLOBAL`/`GETGLOBALV`/`SETGLOBAL`, the
   per-`FuncDef` reference table, assembler and disassembler support.  Follow
   [OPCODE_ADDITION.md](OPCODE_ADDITION.md).
5. Follow-on (optional): resolve free names inside functions to `GETGLOBAL` when
   the code generator can prove they are not enclosing locals.

## 9. Rejected alternatives

**Fix Get/Set to agree and move on.**  Addresses one symptom.  Leaves the
lifetime and extent problems, so `Gather`/`Rebind`/`MapToRegister` and the
per-line stack reallocation all stay, and the next case we hit needs the next
band-aid.

**Give `@main`'s register window stable numbering across REPL lines**, so a
global keeps its register from line to line.  Dies on the 8-bit register field
(256 globals), and does nothing for `globals.x =` at depth, the host API, or a
program that ends and is read afterwards.  It is the "one more band-aid" move.

**Grow `@main`'s register window at run time.**  Compiled code addresses
registers with 8-bit operands and cannot reach new ones, and the window is
per-compilation regardless.

**Keep globals in registers, with the slot table as overflow only.**  This is
what we have.  The whole cost is in the boundary between the two tiers.

**A per-interpreter absolute slot number baked into the instruction**, with no
guard — the compiler asks the live `Globals` for a slot at compile time.  Fastest
possible, but a `FuncDef` compiled in interpreter A would index B's table by A's
numbers, which breaks the cross-interpreter seeding that `HOSTING_MS.md` is
built on.  The id guard costs one compare and keeps that working.
