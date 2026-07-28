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

## MiniScript 2 Implementation

MiniScript 2 uses the alternative described below, with one adjustment.

`Lexer.NextToken` returns `TokenType.STRONG_NEGATE` for a `-` that is preceded by whitespace and not followed by whitespace, and the ordinary `TokenType.MINUS` (the "weak negate" below) otherwise.  "Followed by whitespace" includes tab, newline and end of input, so a trailing `-` is always a `MINUS` and still triggers line continuation.

`Parser` registers `STRONG_NEGATE` as a prefix (negation) parselet, and *also* as an infix (subtraction) parselet — but `GetPrecedence` refuses the infix reading wherever a no-parens call statement could be built:

```csharp
if (_current.Type == TokenType.STRONG_NEGATE && callStatementAllowed) {
    return Precedence.NONE;
}
```

That `callStatementAllowed` flag is the adjustment.  Taking the alternative literally — never letting a `STRONG_NEGATE` be subtraction — breaks `x = a -b`, which 1.x accepts (its `statementStart` is false on an assignment's right-hand side).  So the parser passes `true` only for the outermost expression of a statement, which is exactly where `AtCallArgument` may claim what follows as an argument list; arguments, right-hand sides, conditions and anything inside brackets all recurse through `ParseExpression` and pass `false`.

It is a single parameter on two methods rather than 1.x's threading through every parse method, because in a Pratt parser only `GetPrecedence` has to know.

Coverage: the "UNARY MINUS VS. SUBTRACTION" section of `tests/testSuite.txt`.

## Possible Alternative

Another way to approach this might be to have the tokenizer itself distinguish these as two different tokens:

1. STRONG_NEGATE: <whitespace> `-` <no_whitespace>
2. WEAK_NEGATE: any other `-`

Then, where the grammar looks for subtraction, it would use only WEAK_NEGATE; while where it looks for negation, it would accept either STRONG_NEGATE or WEAK_NEGATE.  This would solve the cases above.  It would disallow syntax like `1 + 2 -3`, but this syntax doesn't work in MiniScript 1.x either (though that is more of a side-effect than in intent).

This might be a simpler model both to implement and to explain.
