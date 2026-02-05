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

## Function Notes

Similar to Python doc strings, we may associate a string with a function definition by having the first statement of the function body evaluate to a string constant.  This constant could be accessible via the funcRef, e.g. as `someFunc.note`, and should be displayed by a REPL when the input expression evaluates to a funcRef.

