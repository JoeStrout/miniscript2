// MiniScript GPLEX Lexer Grammar
// For now, this is a simple expression lexer (will be expanded to full language)

%namespace MiniScript
%using QUT.Gppg;

%%

[0-9]+          { yylval = int.Parse(yytext); return (int)Tokens.NUMBER; }
"+"             { return (int)Tokens.PLUS; }
"-"             { return (int)Tokens.MINUS; }
"*"             { return (int)Tokens.TIMES; }
"/"             { return (int)Tokens.DIVIDE; }
"("             { return (int)Tokens.LPAREN; }
")"             { return (int)Tokens.RPAREN; }
[ \t\r\n]+      { /* skip whitespace */ }
.               { Console.WriteLine("Unexpected character: " + yytext); }

%%
