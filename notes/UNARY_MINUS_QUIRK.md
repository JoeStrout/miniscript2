## Background

MiniScript supports a _call statement_ syntax in which parentheses around the arguments are optional (and discouraged by convention).  This enables us to write things like:
```
print "Hello"
```
as well as (if you insist):
```
print("Hello")
```

But this creates an ambiguity in one specific case: `-`, which acts as both a unary negation operator and a binary subtraction operator.  So, given something like:
```
a - b
```
...this could reasonably be interpreted as "call `a`, passing `-b` as an argument", or as "subtract `b` from `a`".

## The Quirk (MiniScript's Solution)

MiniScript ignores whitespace in most cases, but makes these two exceptions:

1. Whitespace is required in a call statement between the _function reference_ and the _argument(s)_, unless there are parentheses around the whole argument list.

2. An `-` is only interpreted as subtraction if (1) it is not at the start of a statement, and (2) either has whitespace after it, or no whitespace before it.  Otherwise, it can only be negation.

The table below shows some concrete examples.

|Statement|Interpretation|
|---------|--------------|
|`a - b` | subtraction |
|`a- b` | subtraction |
|`a-b` | subtraction |
|`a -b` | `a(-b)` |
|`-b` | negation |
|`- b` | negation |


## MiniScript 1.x Implementation

The MiniScript 1 parser handled this by having the tokenizer set an `afterSpace` flag on each token it returned, indicating whether that token was found after some whitespace.  The tokenizer also provided an `IsAtWhiteSpace()` function to check whether our current position in the token stream was on some whitespace (i.e., whitespace is found after the current token).  Finally, every method in the recursive-descent parser gets a `statementStart` parameter indicating whether we are currently at the start of a statement.  Then, the `ParseAddSub` (addition/subtraction) function simply checks:

```
			while (tok.type == Token.Type.OpPlus || 
					(tok.type == Token.Type.OpMinus
					&& (!statementStart || !tok.afterSpace  || tokens.IsAtWhitespace()))) {
```

This encodes the extra restrictions on OpMinus being interpreted as subtraction.
