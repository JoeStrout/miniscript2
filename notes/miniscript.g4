grammar MiniScript;

// Parser Rules
program
	: (eol | statement)* EOF
	;

eol
    : (WHITESPACE | COMMENT)* NEWLINE
    ;

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
  	elseIfStatement*
  	elseStatement?
  	END IF
	;

elseIfStatement
	: ELSE IF expression THEN NEWLINE
  	(eol | statement)*
	;

elseStatement
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
	: lvalue '=' expression
	;

lvalue
	: IDENTIFIER ('.' IDENTIFIER | '[' expression ']')*
	;

functionBlock
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
    : expression argList
    ;

expressionStatement
	: expression
	;

expression
	: literal                                          	# literalExpr
	| IDENTIFIER                                       	# identifierExpr
	| '(' expression ')'                               	# parenExpr
	| expression '.' IDENTIFIER                        	# dotExpr
	| expression '[' expression ']'                    	# indexExpr
	| expression '[' expression? ':' expression? ']'   	# sliceExpr
	| '@' IDENTIFIER                                   	# funcRefExpr
	| expression '(' argList? ')'                      	# functionCallExpr
	| NEW expression                                 	# newExpr
	| (NOT | '-') expression                         	# unaryExpr
	| expression '^' expression                        	# powerExpr
	| expression ('*' | '/' | '%') expression          	# multDivExpr
	| expression ('+' | '-') expression                	# addSubExpr
	| expression ('<' | '<=' | '>' | '>=' | '==' | '!=') expression  # comparisonExpr
	| expression (AND | OR) expression             	    # logicalExpr
	| functionBlock                                     # functionExpr
	;

argList
	: expression (',' expression)*
	;

literal
	: NUMBER            	# numberLiteral
	| (TRUE | FALSE)        # boolLiteral
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

NEWLINE
	: [\r\n]+
	| ';' 	// Semicolons can be used to separate statements on one line
	;

NUMBER
	: [0-9]+ ('.' [0-9]+)?
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
TRUE    : 'true';
FALSE   : 'false';

IDENTIFIER : ID_START ID_CONTINUE*;

fragment ID_START
    : [a-zA-Z_]
    | [\u00A0-\uFFFF] // Non-ASCII Unicode characters, excluding control chars
    ;

fragment ID_CONTINUE
    : ID_START
    | [0-9]
    ;
