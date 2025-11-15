// MiniScript GPPG Parser Grammar
// For now, this is a simple expression parser (will be expanded to full language)

%namespace MiniScript
%using System;

%token NUMBER PLUS MINUS TIMES DIVIDE LPAREN RPAREN
%left PLUS MINUS
%left TIMES DIVIDE

%%

program : expr
	{ Console.WriteLine("Result: " + $1); }
	;

expr : expr PLUS expr
	{ $$ = $1 + $3; }
	| expr MINUS expr
	{ $$ = $1 - $3; }
	| expr TIMES expr
	{ $$ = $1 * $3; }
	| expr DIVIDE expr
	{ $$ = $1 / $3; }
	| LPAREN expr RPAREN
	{ $$ = $2; }
	| NUMBER
	{ $$ = $1; }
	;

%%
