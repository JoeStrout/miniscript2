# Proposed observable changes in MiniScript 2.0

## Power operator (`^`) is now right-associative

The `^` operator in MiniScript 1.x is left-associative, so `2^3^2` == `(2^3)^2` == 64.  In virtually every other language with a power operator (including Python, JavaScript, Fortran, Ruby, Haskell, and R), as well as in standard math notation, `^` is right-associative, so `2^3^2` == `2^(3^2)` == 512.

The left-associativity of `^` in MiniScript 1.0 was a mistake.  In MiniScript 2.0, it will be right-associative, like most other languages.  **This is a code-breaking change**, though not one that will affect most users.

## Dot syntax on numeric literals

MiniScript 1.0 does not allow things like `4.foo` or `3.14.foo`, even when `foo` is defined as an extension method on the `number` map.  You can call such methods on variables (e.g. `x.foo`), but not on numeric literals.  But there's no strong reason to disallow it, and users are occasionally surprised when it doesn't work.  So, in 2.0, let's allow it.

## Frozen Maps and Lists

See [FROZEN_VALUES.md](FROZEN_VALUES.md).

## Function Expressions

`function`...`end function` will comprise an _expression_, not a statement.  This just cleans up various odd corners of the syntax.  The effect of this expression is still to create a funcRef, with code that is compiled (just once) for whatever's between the keywords, and `outer` (if needed) assigned to the locals of the function evaluating this expression.

## Map Iteration Order

MiniScript documentation has always said that the order of data in a map is undefined.  In practice, though, the C# version of MiniScript always returned keys in insertion order, because that is how the underlying Dictionary class works.

In MiniScript 2, we will commit to this behavior: **maps return keys in insertion order**, and this is true in both the C# and the C++ versions.

## String Indexing

**A string is indexed in Unicode code points**, and `len` counts them.  This is true in both the C# and the C++ versions, and it is the rule for every operation that takes or returns a character index — `s[i]`, slices, `indexOf` (both its start position and its result), `insert`, `values`, `indexes`, `split("")`, and a fractional `*`.

**The consistency is new.**  MiniScript 1's two implementations disagreed: the C++ one counted characters (`SimpleString::Length()` returns a `charCount` analyzed from the UTF-8 bytes), while the C# one never handled surrogate pairs at all, so it counted UTF-16 code units and a string containing an emoji indexed differently on the two.  MiniScript 2 commits to the C++ reading on both sides.

It is worth writing down because *nothing* in the implementations makes the rule obvious, and each one is one careless line away from breaking it in a way the other would not:

- **C++ stores UTF-8.**  A character is 1 to 4 bytes, so a byte index and a character index differ for any non-ASCII string at all.  Code that confuses the two breaks on `é`, which is conspicuous.
- **C# stores UTF-16.**  A character in the Basic Multilingual Plane is one code unit, so a code-unit index and a character index agree for `é`, `日`, `ü` and almost everything else anyone tests with.  They diverge only at a character outside the BMP — an emoji, say — which is stored as a *surrogate pair* of two code units.  Code that confuses the two therefore looks entirely correct until one shows up.

That asymmetry is exactly how MiniScript 1's C# side got away with it for years, and how [bugs.md](bugs.md) entry 13 stayed hidden here: the whole test suite passed, because its non-ASCII coverage used only BMP characters.  Anything added to `cs/Value.cs` that touches a character index should go through its `CharToUnit` / `UnitToChar` helpers, never through `s.Length` or `s[i]` — and any new test for string indexing should include an astral character, not just an accented one.

Note that a code point is not a user-perceived character.  `é` also has a two-code-point form (`e` followed by U+0301 combining acute), and an emoji carrying a skin tone or joined with ZWJ is several code points; all of these count as more than one character.  Grapheme-cluster semantics are deliberately out of scope, as they were in MiniScript 1.

## Function Info

`info(x)` returns a frozen map describing any value.  Every result carries `type`, the same string `typeof` would give.  Beyond that it depends on what `x` is:

- **funcRef** — `name`, the text of the expression to the left of `=` where the function was defined, if any; `note`, the string constant the first statement of the body evaluates to, if it does (similar to a Python docstring); `params`, a list of little maps, one per parameter, each with `name` and `default` (the actual default value); `closure`, 1 if the function captured an enclosing scope; and `sourceLoc`, where the function was defined, as a string in the same `"{file} line {n}"` form a stack trace entry uses (the line is that of the `function` keyword).  `sourceLoc` is null for a function with no source of its own — a built-in, or one loaded from assembly.
- **list** — `computed`, 1 if the list is still in its lazily-computed form (see [adr/0007-computed-lists.md](adr/0007-computed-lists.md)); and `frozen`.
- **map** — `frozen`.
- **error** — `message`, `inner`, `stack`, and `isa`.

## Error Type

We'll add one new type to MiniScript, `error`, which represents a runtime error, and a new `err(msg, innerErr=null)` intrinsic for creating them.  Here are the special rules, given an error `e` and any type `x`:

- Global intrinsic `err(msg, e)` returns a new `error` (let's call it `e2`) such that `e2.message == msg`,  `e2.inner == e`, and `e2.stack` returns the stack trace at this point.
- `se.err(msg, e)` does the same, but also sets `e2.__isa == se`, i.e., it creates a specialization of a more general error `se`.  The `__isa` chain can be probed with the `isa` operator, just like with maps.  Note that this `.err` method will terminate if it creates a *loop* in the `__isa` chain.  The same rule applies to maps, but is enforced at the only place a loop can actually be made: assigning to a map's `__isa` key terminates if the assignment would close a cycle.  (`new` needs no check of its own -- it always allocates a fresh map, which nothing can already point at -- but it does require its operand to be a map, and returns an error value if it is not.)
- `e.foo` (where `foo` is _not_ literally `message`, `inner`, `stack`, or `__isa`) terminates [note 1].
- All errors are immutable, so attempting `e.message = rhs`, etc. will terminate with a runtime error.
- `e or x` evaluates to `x`.
- `if e then` and `while e` both terminate.
- Passing `e` to an intrinsic `f` never makes it vanish silently.  By default `f(e)` evaluates to `e` without running `f` at all, or terminates if `f` affects state (or doesn't normally return a result) [note 2].  The exceptions are parameters for which an error is a meaningful argument: `print` and `input` show it, `info` describes it, `err` takes it as the inner error, `hash` and `refEquals` use its identity, and `push`, `insert`, and `replace` can store it in a list or map.
- When `e` is the thing searched for -- the value in `indexOf`, the index in `hasIndex` and `remove`, the old value in `replace` -- the search runs normally, since errors may be stored in lists and maps.  But if `e` is not found, the result is `e` itself rather than null or 0 (or, for `remove` and `replace`, which modify their receiver, the program terminates).
- An error inside a list or map given to `sum` makes the sum `e`, as `+` would.
- All binary operators except for `isa`, `==`, and `!=` (so `+`, `-`, `*`, `/`, `%`, `^`, `<`, `<=`, `>`, `>=`, `and`) involving `e` evaluate to `e` (or if both operands are errors, evaluate to the first one), as do the unary operators `-` and `not`.
- `e[i]` evaluate to `e`.
- Any use of `e.foo` or `e[i]` in an lvalue expression terminates.
- An expression statement that evaluates to `e` terminates, since the value is discarded and so could never be caught.  This is what makes a failing `import "foo"` (or `f.readLines(path)`, etc.) halt on its own line, while `x = import("foo")` keeps the error as an ordinary value.

Note 1: "Terminates" above means that the program is halted, and a new runtime error is displayed, containing `e` as its `inner` error.  Program code can not catch these; but they are always preventable with proper code.
Note 2: Any function can be written to propagate an argument error, or do something else when sensible; user functions are up to the user.

And here are some ways in which errors are perfectly ordinary:
- `return e` simply returns `e` to the caller (i.e., ordinary `return` behavior, nothing special here).
- `e isa error` evaluates to 1; `isa` with any other type evaluates to 0.
- `e == x` evaluates to 0 (unless `x` is, in fact, another reference to `e`).
- `e != x` evaluates to 1 (unless `x` is, in fact, another reference to `e`).
- `@e` evaluates to `e`.
- `x = e` makes `x` another reference to `e`.
- `e` may be stored in lists and maps (including as map keys).






## Property Setters

MiniScript 1.x had `assignOverride`: a function stored on a map, which was consulted on every assignment into that map and could cancel the store by returning true.  It was not inherited — a map made with `new` did not get its prototype's override — which forced a pile of work-arounds in Mini Micro, and it was all-or-nothing, so the override ran on every key whether it cared about that key or not.

MiniScript 2 replaces it with **property setters**, which fall out of a symmetry the language already had.

Reading a member already hides the difference between storage and computation: `foo.x` returns the stored value if `x` holds data, and *calls* it if `x` holds a function of no arguments.  So a getter is just a member named `x`.  The hiding was one-way, though; there was no matching story for writing.  Now there is:

> **A member named `x` answers reads of `x`.  A member named `x=` answers writes to `x`.**

When `foo.x = v` (or `foo["x"] = v`) executes, the `__isa` chain of `foo` is searched for the key `"x="`, exactly as a method lookup searches it:

- **Not found** — the value is stored normally.  This is the overwhelmingly common case and costs one failed lookup.
- **A function** — it is called with `v` as its one argument and `self` bound to `foo` (the object written to, not the map the setter was found on).  Nothing is stored; storing is now the setter's job.  The return value is discarded — *unless it is an error*, which halts the program at the assignment, since a discarded error is one nobody could ever catch.  That is the same rule a bare expression statement follows, and it is enforced the same way, with an `ERRCHK` after the call.
- **`null`** — the property is read-only: the program terminates with a runtime error naming the property.
- **Anything else** — a runtime error: the setter is invalid.

Since the lookup is an ordinary `__isa` walk, setters are **inherited and overridable** like any other member, and the most-derived one wins.

### Defining a setter

`x=` is not a legal identifier, so a setter is installed with the bracket form:

```
Sprite = {}
Sprite["x="] = function(value)
    self.__x = value
    // ...tell the renderer something moved...
end function
```

That is deliberate.  Setters are an advanced, library-author feature, not something a typical user should reach for, and requiring the brackets keeps them visible as such while costing the language no new syntax.

### Doing the ordinary store

A setter that wants the normal storage to happen — after validating, or purely as a side channel — writes `super.x = value`.  That resumes the setter search *above* the map the running setter was found on, and if nothing is found there, performs the plain store into `self`.  So it serves both as "delegate to my parent's setter" and as the base case:

```
Sprite["x="] = function(value)
    if value isa string then return   // silently ignore bad input
    super.x = value                   // the ordinary store
end function
```

`self.x = value` inside a setter would of course find the same setter again and recurse until the call stack overflows.

**This changes what `super.x = v` means.**  It previously stored into the parent map; now the store lands on `self`.  Storing through `super` into a prototype was never a useful thing to do, and the new reading is the one that makes the base case work.

### Read-only properties

A module that exposes computed properties usually wants them protected.  Without protection, a user who guesses wrong — `mouse.x = 42`, hoping to move the cursor — silently clobbers the `x` getter and has no way to get it back.  Assigning `null` as the setter turns that into an immediate, clear error:

```
mouse["x="] = null       // mouse.x is now read-only
```

`null` was chosen over a stock "raise an error" function for two reasons.  It is legible: someone printing the map can tell at a glance which properties are actively managed and which are merely closed, whereas a sentinel function is indistinguishable from a real setter.  And it gives *better* errors, not worse — no setter is called, so the VM raises the error itself, and it has the key in hand and can name the property.

### Rules and boundaries

- **A setter reports failure by returning an error.**  `return err("...")` from a setter halts the program with that error, attributed to the assignment that triggered it.  There is no way to veto an assignment quietly and have the caller find out — a setter either handles the write, ignores it, or fails loudly.
- **Only map member and index assignment is intercepted.**  `x = 5` on a plain variable is not a map store and never consults a setter.  Nor does `remove foo.x`.
- **`foo.x = v` and `foo["x"] = v` behave identically**, including when the key is computed: `foo[k] = v` looks for `k + "="`.  There is no bracket form that quietly bypasses a setter.
- **A key that already ends in `=` is never intercepted**, so installing a setter does not go looking for `"x=="`.
- **Only the `__isa` chain is searched, not the type maps.**  Reads fall back to `map`, `list`, `string` and `number`; writes deliberately do not.  A `"x="` entry on the shared `map` type would otherwise intercept assignment to *every* map in the program, which is a far bigger hammer than the read-side fallback, and one that could silently swallow stores rather than merely adding a method.
- **Frozen wins.**  A frozen map raises its usual error before any setter is consulted.  `freeze` keeps its plain meaning — this object cannot change — which matters because frozen values are what `frozenCopy` produces for map keys; a setter firing on a frozen map would let assignment to a live map *key* run arbitrary code.
- **There is no way to un-inherit a setter.**  A subclass can replace an inherited setter with its own, or close the property with `null`, but it cannot restore plain storage for a key its prototype manages.  This has not come up; if it ever does, it wants a deliberate design, not a second magic value.
- **No wildcard.**  There is no catch-all that sees keys with no setter defined.  If one is ever genuinely needed, a reserved key (`"?="`, with `"?"` as its read-side twin) is the shape it would take, but the expectation is that it never is.
- **Setters installed from host code are not seen** unless the host tells the VM about them.  Whether a map needs setter dispatch at all is cached per map (see *Performance* below), and the cache is kept current by the assignment path in the VM; a native `MapSet("x=", ...)` goes around it.  A host installing a setter directly must call `VM.NoteSetterDefined(map)`, and one removing a key directly must call `VM.NoteKeyRemoved(key)`.

### Performance

Every map member/index assignment has to answer "does this map's `__isa` chain
hold a setter?" before it can store anything, and the answer has to be cheap,
because in a real environment (Mini Micro, say) *some* map somewhere will always
have one.  So the answer is cached per map rather than per program.

A global `SetterGeneration` counter is bumped whenever anything could change an
answer: a setter key stored or removed, or an `__isa` link rewired.  Each map
carries a `_setterStatus`:

- **-1** — this map itself holds at least one key ending in `=`.  Written when
  such a key is *stored*, never discovered by searching: answering the question
  by scanning a map's keys would cost more than the lookup it is avoiding.
- **0** — nothing known, except that no setter key has ever been stored here.
- **anything else** — the generation at which this map's whole chain was walked
  and found to hold no setter at all.

An assignment walks the chain only until some map answers for the rest of it —
a -1, or a stamp equal to the current generation.  When the walk comes out
clean it stamps every map it passed, so a chain settles after one walk and
stays settled until a setter or an `__isa` link actually moves.  Bumping the
generation retires every stamp at once, which is what makes this safe: maps
have no back-pointers, so there is no way to find the descendants of a map that
just gained a setter and invalidate them individually.  A stale stamp is
therefore never wrong, only slow, and the next walk replaces it.

Building the `key + "="` string is the other cost, and it falls on every
assignment to a map that has any setter.  `Value.SetterKey` does it with pure
bit arithmetic whenever the result still fits a tiny string: a tiny string keeps
byte *i* at bit 8*(i+1) and its length in the low byte, with the slots above the
length left zero and the tag confined to bits 48-63, so appending one ASCII byte
is "bump the length, drop 0x3D into the next slot" — no UTF-8 decode, no
allocation, no intern-table lookup.  That covers every property name up to four
UTF-8 bytes; longer ones fall back to building the string.  (Going through
`AsCString()` instead would allocate on every assignment, and does not even
compile on the C++ side, where it returns a `const char*`.)

Measured on 1.2M map assignments: a program with no setters anywhere and a
program with a setter on an unrelated map run identically (~0.92s vs ~0.97s),
which is the whole point of caching per map instead of per program.  Assigning
to a map that *does* have a setter — even on a different key — costs about a
third more (~1.23s), which is the chain lookup for the setter key.  That cost is
confined to the maps actually using the feature.

Two subtleties worth keeping in mind when touching this:

- `new` sets `__isa` without bumping the generation, and that is correct: the
  map it is writing to was allocated a moment earlier, so nothing else can have
  it in a chain and no stamp can go stale.  The same holds for the `RawData` and
  `FileHandle` instances `ShellIntrinsics` builds.
- The -1 is deliberately sticky.  Removing a setter key cannot clear it without
  scanning for other setter keys, so a map that once held one keeps paying for a
  lookup.  That is the conservative direction, and removing a setter is not
  something programs do in a loop.
