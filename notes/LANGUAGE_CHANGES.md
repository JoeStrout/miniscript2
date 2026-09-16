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

- **funcRef** — `name`, the text of the expression to the left of `=` where the function was defined, if any; `note`, the string constant the first statement of the body evaluates to, if it does (similar to a Python docstring); `params`, a list of little maps, one per parameter, each with `name` and `default` (the actual default value); and `closure`, 1 if the function captured an enclosing scope.
- **list** — `computed`, 1 if the list is still in its lazily-computed form (see [adr/0007-computed-lists.md](adr/0007-computed-lists.md)); and `frozen`.
- **map** — `frozen`.
- **error** — `message`, `inner`, `stack`, and `isa`.

Still to do: **`sourceLoc`**, the location of a function definition in the source code, which was part of the original design and is not yet returned.

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





