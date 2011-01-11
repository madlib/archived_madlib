/* -----------------------------------------------------------------------------
 * sql.y
 * 
 * A simple context-free grammar to parse SQL files and translating
 * CREATE FUNCTION statements into C++ function declarations.
 * This allows .sql files to be documented by documentation tools like Doxygen.
 *
 * Revision History:
 * 0.1: Florian Schoppmann, Jan. 2011, Initial version.
 * -----------------------------------------------------------------------------
 */

%{
	#include <stdlib.h>
	#include <stdio.h>
	#include <string.h>

	#define YYSTYPE char *
	
	extern FILE *yyin;
	
	void yyerror (char const *);
%}

%token IDENTIFIER

%token COMMENT

%token CREATE_FUNCTION

%token IN
%token OUT
%token INOUT

%token RETURNS
%token SETOF

%token AS
%token LANGUAGE
%token IMMUTABLE
%token STABLE
%token VOLATILE
%token CALLED_ON_NULL_INPUT
%token RETURNS_NULL_ON_NULL_INPUT
%token SECURITY_INVOKER
%token SECURITY_DEFINER

/* types with more than 1 word */
%token BIT
%token CHARACTER
%token DOUBLE
%token PRECISION
%token TIME
%token VARYING 
%token VOID
%token WITH
%token WITHOUT 
%token ZONE

%token INTEGER_LITERAL
%token STRING_LITERAL

%% /* Grammar rules and actions follow. */

input:
	| input stmt
	| input COMMENT { printf("//%s\n", $2); }
	| input '\n' { printf("\n"); }
;

stmt:
	  ';'
	| createFnStmt ';' { printf(";\n\n"); }
;

createFnStmt:
	  CREATE_FUNCTION qualifiedIdent '(' optArgList ')' returnDecl fnOptions {
		printf("%s %s(%s) { }", $6, $2, $4);
	}
;

qualifiedIdent:
	  IDENTIFIER
	| IDENTIFIER '.' IDENTIFIER {
		$$ = $3;
		/* asprintf(&($$), "%s::%s", $1, $3); */
	}
;

optArgList:
	| argList
;

argList:
	  argument
	| argList ',' argument {
		asprintf(&($$), "%s, %s", $1, $3);
	}
;

argument:
	  type
	| argname type {
		asprintf(&($$), "%s %s", $2, $1);
	}
	| argmode argname type {
		asprintf(&($$), "%s %s", $3, $2);
	}
;

argmode:
	  IN
	| OUT
	| INOUT
;

argname:
	IDENTIFIER
;

type:
	  baseType optArray {
		asprintf(&($$), "%s%s", $1, $2);
	}
;

baseType:
	  qualifiedIdent
	| BIT VARYING optLength {
		asprintf(&($$), "varbit%s", $3);
	}
	| CHARACTER VARYING optLength {
		asprintf(&($$), "varchar%s", $3);
	}
	| DOUBLE PRECISION { $$ = "float8"; }
	| VOID { $$ = "void"; }
;

optArray: { $$ = ""; }
	| array;

optLength:
	| '(' INTEGER_LITERAL ')' {
		asprintf(&($$), "(%s)", $2);
	}
;

array:
	  '[' ']' { $$ = "[]"; }
	| '[' INTEGER_LITERAL ']' {
		asprintf(&($$), "[%s]", $2);
	}
	| array '[' ']' {
		asprintf(&($$), "%s[]", $1);
	}
	| array '[' INTEGER_LITERAL ']' {
		asprintf(&($$), "%s[%s]", $1, $2);
	}
;

returnDecl: { $$ = "void"; }
	| RETURNS retType { $$ = $2; }
;

retType:
	  type
	| SETOF type {
		asprintf(&($$), "set<%s>", $2);
	}
;

fnOptions:
	| fnOptions fnOption;

fnOption:
	  AS STRING_LITERAL
	| AS STRING_LITERAL ',' STRING_LITERAL
	| LANGUAGE STRING_LITERAL
	| LANGUAGE IDENTIFIER
	| IMMUTABLE
	| STABLE
	| VOLATILE
	| CALLED_ON_NULL_INPUT
	| RETURNS_NULL_ON_NULL_INPUT
	| SECURITY_INVOKER
	| SECURITY_DEFINER
;

%%

void yyerror (char const *s) {
	fprintf (stderr, "%s\n", s);
}

int	main(int argc, char **argv)
{
	if (argc > 1)
		yyin = fopen(argv[1], "r");
	else
		yyin = stdin;

	yyparse();
	return 0;
}
