# Proposed observable changes in MiniScript 2.0

## Power operator (`^`) is now right-associative

The `^` operator in MiniScript 1.x is left-associative, so `2^3^2` == `(2^3)^2` == 64.  In virtually every other language with a power operator (including Python, JavaScript, Fortran, Ruby, Haskell, and R), as well as in standard math notation, `^` is right-associative, so `2^3^2` == `2^(3^2)` == 512.

The left-associativity of `^` in MiniScript 1.0 was a mistake.  In MiniScript 2.0, it will be right-associative, like most other languages.  **This is a code-breaking change**, though not one that will affect most users.

## Dot syntax on numeric literals

MiniScript 1.0 does not allow things like `4.foo` or `3.14.foo`, even when `foo` is defined as an extension method on the `number` map.  You can call such methods on variables (e.g. `x.foo`), but not on numeric literals.  But there's no strong reason to disallow it, and users are occasionally surprised when it doesn't work.  So, in 2.0, let's allow it.

## Frozen Maps and Lists

See [LANGUAGE_CHANGES.md](LANGUAGE_CHANGES.md).

## Function Expressions

`function`...`end function` will comprise an _expression_, not a statement.  This just cleans up various odd corners of the syntax.  The effect of this expression is still to create a funcRef, with code that is compiled (just once) for whatever's between the keywords, and `outer` (if needed) assigned to the locals of the function evaluating this expression.

## Map Iteration Order

MiniScript documentation has always said that the order of data in a map is undefined.  In practice, though, the C# version of MiniScript always returned keys in insertion order, because that is how the underlying Dictionary class works.

In MiniScript 2, we will commit to this behavior: **maps return keys in insertion order**, and this is true in both the C# and the C++ versions.

## Function Info

FuncRef will get some metadata which can be accessed via a new intrinsic, perhaps like `info(@f)`.  This will include:

- name: the text of the expression to the left of `=` where the function was defined, if any
- note: when the first statement of a function body evaluates to a string constant, it is stored as the note (similar to a Python docstring)
- params: a list of little maps, one for each parameter; each contains `name` and `default` (the actual default value)
- sourceLoc: the location of this function definition in the source code

These details will be returned by `info` as a frozen map.

## Error Type

We'll add one new type to MiniScript, `error`, which represents a runtime error, and a new `err(msg, innerErr=null)` intrinsic for creating them.  Here are the special rules, given an error `e` and any type `x`:

- Global intrinsic `err(msg, e)` returns a new `error` (let's call it `e2`) such that `e2.message == msg`,  `e2.inner == e`, and `e2.stack` returns the stack trace at this point.
- `se.err(msg, e)` does the same, but also sets `e2.__isa == se`, i.e., it creates a specialization of a more general error `se`.  The `__isa` chain can be probed with the `isa` operator, just like with maps.  Note that this `.err` method will terminate if it creates a *loop* in the `__isa` chain.  (`new` should also.)
- `e.foo` (where `foo` is _not_ literally `message`, `inner`, `stack`, or `__isa`) terminates [note 1].
- All errors are immutable, so attempting `e.message = rhs`, etc. will terminate with a runtime error.
- `e or x` evaluates to `x`.
- `if e then` and `while e` both terminate.
- `f(e)` returns `e` for any intrinsic pure function `f` [note 2]; functions that affect state (or which don't normally return a result) terminate.
- All binary operators except for `isa`, `==`, and `!=` (so `+`, `-`, `*`, `/`, `%`, `^`, `<`, `<=`, `>`, `>=`, `and`) involving `e` evaluate to `e` (or if both operands are errors, evaluate to the first one), as do the unary operators `-` and `not`.
- `e[i]` evaluate to `e`.
- Any use of `e.foo` or `e[i]` in an lvalue expression terminates.

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





