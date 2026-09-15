grammar MiniScript;

// NON-NORMATIVE.  The language is actually defined by the hand-written lexer
// and Pratt parser (cs/Lexer.cs, cs/Parser.cs, cs/Parselet.cs, with the token
// set and precedence table in cs/LangConstants.cs).  This grammar is a reading
// aid, kept roughly in step with those; where they disagree, they win.
//
// Two things in MiniScript resist an ANTLR grammar entirely, and are described
// in prose at the bottom of this file rather than encoded here: the call
// statement with no parentheses, and the unary-minus rule that follows from it.

// Parser Rules
program
	: (eol | statement)* EOF
	;

eol : NEWLINE;

statement
	: (simpleStatement
	| ifBlock
	| whileBlock
	| forBlock
	| expressionStatement) eol
	;

simpleStatement
    : singleLineIf
    | callStatement
	| assignmentStatement
	| breakStatement
	| continueStatement
	| returnStatement
	| expressionStatement
    ;

singleLineIf
    : IF expression THEN simpleStatement (ELSE simpleStatement)?
    ;

ifBlock
	: IF expression THEN NEWLINE
  	(eol | statement)*
  	elseIfClause*
  	elseClause?
  	END IF
	;

// Note that an `else if` chain is parsed as a flat list of clauses, not as a
// nested `if` inside the `else` -- one `end if` closes the whole chain.
elseIfClause
	: ELSE IF expression THEN NEWLINE
  	(eol | statement)*
	;

elseClause
	: ELSE NEWLINE
  	(eol | statement)*
	;

whileBlock
	: WHILE expression NEWLINE
  	(eol | statement)*
  	END WHILE
	;

forBlock
	: FOR IDENTIFIER IN expression NEWLINE
  	(eol | statement)*
  	END FOR
	;

breakStatement
	: BREAK
	;

continueStatement
	: CONTINUE
	;

assignmentStatement
	: lvalue assignOp expression
	;

assignOp
	: '=' | '+=' | '-=' | '*=' | '/=' | '%=' | '^='
	;

lvalue
	: (IDENTIFIER | SELF | SUPER | LOCALS | OUTER | GLOBALS)
	  ('.' IDENTIFIER | '[' expression ']')*
	;

// `function` is an expression, not a statement: it may appear anywhere an
// expression may, and is most often the right-hand side of an assignment.
functionExpr
	: FUNCTION paramList? NEWLINE
  	(eol | statement)*
  	END FUNCTION
	;

paramList
	: '(' (param (',' param)*)? ')'
	;

param
	: IDENTIFIER ('=' expression)?
	;

returnStatement
	: RETURN expression?
	;

callStatement
	: expression '(' argList ')'
    | expression argList          // see "The call statement" below
	;

expressionStatement
	: expression
	;

// Listed loosest-binding first; this mirrors the Precedence enum in
// cs/LangConstants.cs.  `^` is right-associative (unlike MiniScript 1.x);
// everything else binary is left-associative.  Note that `isa` binds *more*
// tightly than the comparison operators, and `not` sits between `and` and
// the equality operators.
expression
	: expression (AND | OR) expression                 	# logicalExpr
	| NOT expression                                    # notExpr
	| expression ('==' | '!=') expression              	# equalityExpr
	| expression ('<' | '<=' | '>' | '>=') expression  	# comparisonExpr
	| expression ISA expression                        	# isaExpr
	| expression ('+' | '-') expression                	# addSubExpr
	| expression ('*' | '/' | '%') expression          	# multDivExpr
	| '-' expression                                    # negateExpr
	| <assoc=right> expression '^' expression          	# powerExpr
	| expression '.' IDENTIFIER                        	# dotExpr
	| expression '[' expression ']'                    	# indexExpr
	| expression '[' expression? ':' expression? ']'   	# sliceExpr
	| expression '(' argList? ')'                      	# functionCallExpr
	| NEW expression                                 	# newExpr
	| '@' expression                                   	# funcRefExpr
	| '(' expression ')'                               	# parenExpr
	| literal                                          	# literalExpr
	| IDENTIFIER                                       	# identifierExpr
	| SELF                                              # selfExpr
	| SUPER                                             # superExpr
	| LOCALS                                            # localsExpr
	| OUTER                                             # outerExpr
	| GLOBALS                                           # globalsExpr
	| functionExpr                                      # functionLiteral
	;

argList
	: expression (',' expression)*
	;

// There are no boolean literals: `true` and `false` are ordinary identifiers
// bound to 1 and 0.  Likewise `null`, `pi`, and the type names `string`,
// `number`, `list`, `map` and `funcRef`.
literal
	: NUMBER            	# numberLiteral
	| STRING            	# stringLiteral
	| listLiteral       	# listLit
	| mapLiteral        	# mapLit
	;

listLiteral
	: '[' (expression (',' expression)*)? ']'
	;

mapLiteral
	: '{' (mapEntry (',' mapEntry)*)? '}'
	;

mapEntry
	: expression ':' expression
	;

// Lexer Rules
// (Note that COMMENT and WHITESPACE are sent to the HIDDEN channel,
// which makes the parser rules generally ignore them unless they are
// explicitly called for.)
COMMENT
	: '//' ~[\r\n]* -> channel(HIDDEN)
	;

WHITESPACE
	: [ \t]+ -> channel(HIDDEN)
	;

// A semicolon is lexed as an end-of-line, so it separates statements on one
// physical line.  A trailing binary operator continues a statement onto the
// next line.
NEWLINE : (';' | '\r'? '\n' | '\r')+;

// A digit is always required before the decimal point: `.5` is not a number.
NUMBER
	: [0-9]+ ('.' [0-9]+)? ([eE] [+-]? [0-9]+)?
	;

STRING
	: '"' ( '""' | ~["] )* '"'
	;

IF  	: 'if';
ELSE	: 'else';
WHILE   : 'while';
FOR 	: 'for';
IN  	: 'in';
FUNCTION: 'function';
RETURN  : 'return';
END 	: 'end';
BREAK   : 'break';
CONTINUE: 'continue';
NEW 	: 'new';
THEN	: 'then';
AND     : 'and';
OR      : 'or';
NOT     : 'not';
ISA     : 'isa';
SELF    : 'self';
SUPER   : 'super';
LOCALS  : 'locals';
OUTER   : 'outer';
GLOBALS : 'globals';

IDENTIFIER : ID_START ID_CONTINUE*;

fragment ID_START
    : [a-zA-Z_]
    | [ -￿] // Non-ASCII Unicode characters, excluding control chars
    ;

fragment ID_CONTINUE
    : ID_START
    | [0-9]
    ;

// ===========================================================================
// What this grammar cannot say
// ===========================================================================
//
// The call statement.  `print "hi"` is a call with the parentheses left off,
// which is why `callStatement` has an `expression argList` alternative.  That
// alternative is wildly ambiguous in isolation: `f -1` could be a call or a
// subtraction, and `f [1]` could be a call or an index.  The real parser
// resolves it with a `callStatementAllowed` flag that is true only for the
// outermost expression of a statement -- exactly where an argument list may be
// claimed -- and false inside arguments, right-hand sides, conditions and
// brackets.
//
// Unary minus.  Falling out of the above, the lexer returns a distinct token
// for a `-` that is preceded by whitespace and not followed by it, and the
// parser refuses to read that token as subtraction wherever a parenthesis-less
// call statement could be built.  So `a - b`, `a- b` and `a-b` subtract, while
// `a -b` calls `a` with `-b`.  See UNARY_MINUS_QUIRK.md.
//
// Dot on a numeric literal.  `4.foo` and `3.14.foo` are method calls on a
// number, so the lexer has to decide where the NUMBER ends and the `.` begins.
//
// `end`.  There is one END token; `end if`, `end while`, `end for` and
// `end function` are recognized by the parser, not the lexer.
