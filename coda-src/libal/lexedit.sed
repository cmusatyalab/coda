# sed script to edit lex.yy.c
# This gross hack is needed because lex generates old-style definitions
# of the functions yyback(p, m), yyoutput(c), and yyunput(c)
# Alas, g++ does not accept old-style function definitions
# Someone should fix lex and bring it up to the modern age....

s/^yyback(p, m)/yyback(int \*p, int m)/
tskpone
s/^yyoutput(c)/yyoutput(int c) {/
tskpone
s/^yyunput(c)/yyunput(int c) {/
tskpone
b

:skpone
n
d
