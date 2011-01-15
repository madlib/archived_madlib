/* -----------------------------------------------------------------------------
 * sql.l
 *
 * A simple flex file for use in companion with sql.y. Use case: Documenting
 * .sql files with tools like Doxygen.
 * 
 * Revision History:
 * 0.1: Florian Schoppmann, Jan. 2011, Initial version.
 * -----------------------------------------------------------------------------
 */

/* Definitions */

%option noyywrap

/* C Code */
%{
	#include <string.h>
	
	#define YYSTYPE char *

	#include "sql.tab.h"

	int		stringCaller;
	char	*stringLiteralQuotation = NULL;
%}

/* Definitions */
CREATE_FUNCTION "CREATE"{SPACE}("OR"{SPACE}"REPLACE"{SPACE})?"FUNCTION"
CREATE_AGG "CREATE"{SPACE}"AGGREGATE"
COMMENT "--".*$
IDENTIFIER [[:alpha:]_][[:alnum:]_]*
INTEGER [[:digit:]]+
SPACE [[:space:]]+
DOLLARQUOTE "$$"|"$"{IDENTIFIER}"$"


/* State definitions */

%s sAFTER_COMMENT
%s sFUNC_DECL
%s sFUNC_ARGLIST
%s sFUNC_OPTIONS
%x sSTRING_LITERAL
%x sDOLLAR_STRING_LITERAL



%%	/* Rules */

	/* Contiguity of comment blocks is meaningful and therefore has to be
	 * preserved. Note that input . is handled below */
<sAFTER_COMMENT>\n[[:space:]]* {
	BEGIN(INITIAL);
	return '\n';
}

	/* Ignore spaces */
{SPACE}

{COMMENT} {
	yylval = malloc(yyleng - 1);
	strcpy(yylval, yytext + 2);
	BEGIN(sAFTER_COMMENT);
	input(); /* consume the newline character */
	return COMMENT;
}

	/* String literals in single quotes */
"'" { stringCaller = YY_START; BEGIN(sSTRING_LITERAL); }

<sSTRING_LITERAL>{
	"''" { yymore(); }
	"\\'" { yymore(); }
	"'" {
		yytext[yyleng - 1] = '\0';
		yylval = strdup(yytext);
		BEGIN(stringCaller);
		if (stringCaller != INITIAL && stringCaller != sAFTER_COMMENT)
			return STRING_LITERAL;
	}
	. { yymore(); }
}

	/* String literals in dollar quotes, see
	http://www.postgresql.org/docs/current/static/sql-syntax-lexical.html#SQL-SYNTAX-DOLLAR-QUOTING */
{DOLLARQUOTE} {
	stringCaller = YY_START;
	stringLiteralQuotation = malloc(yyleng - 1);
	strncpy(stringLiteralQuotation, yytext + 1, yyleng - 1);
	BEGIN(sDOLLAR_STRING_LITERAL);
}

<sDOLLAR_STRING_LITERAL>{
	{DOLLARQUOTE} {
		if (strncmp(yytext + 1, stringLiteralQuotation, yyleng - 1) == 0) {
			yylval = "<omitted by lexer>";
			BEGIN(stringCaller);
			free(stringLiteralQuotation);
			stringLiteralQuotation = NULL;
			if (stringCaller != INITIAL && stringCaller != sAFTER_COMMENT)
				return STRING_LITERAL;
		} else {
			yymore();
		}
	}
	.|\n
}

{CREATE_AGG} { BEGIN(sFUNC_DECL); return CREATE_AGG; }

{CREATE_FUNCTION} { BEGIN(sFUNC_DECL); return CREATE_FUNCTION; }

<sFUNC_DECL>"(" { BEGIN(sFUNC_ARGLIST); return '('; }
<sFUNC_ARGLIST>")" { BEGIN(sFUNC_OPTIONS); return ')'; }

	/* We disallow using the following keywords as argument names */
<sFUNC_ARGLIST,sFUNC_OPTIONS>{
	"IN" return IN;
	"OUT" return OUT;
	"INOUT" return INOUT;
	
	"BIT" return BIT;
	"CHARACTER" return CHARACTER;
	"DOUBLE" return DOUBLE;
	"PRECISION" return PRECISION;
	"TIME" return TIME;
	"WITH" return WITH;
	"WITHOUT" return WITHOUT;
	"VOID" return VOID;
	"VARYING" return VARYING;
	"ZONE" return ZONE;

	"RETURNS" return RETURNS;
	"SETOF" return SETOF;
	
	"AS" return AS;
	"LANGUAGE" return LANGUAGE;
	"IMMUTABLE" return IMMUTABLE;
	"STABLE" return STABLE;
	"VOLATILE" return VOLATILE;
	"CALLED"{SPACE}"ON"{SPACE}"NULL"{SPACE}"INPUT" return CALLED_ON_NULL_INPUT;
	"RETURNS"{SPACE}"NULL"{SPACE}"ON"{SPACE}"NULL"{SPACE}"INPUT"|"STRICT" {
		return RETURNS_NULL_ON_NULL_INPUT; }
	("EXTERNAL"{SPACE})?"SECURITY"{SPACE}"INVOKER" return SECURITY_INVOKER;
	("EXTERNAL"{SPACE})?"SECURITY"{SPACE}"DEFINER" return SECURITY_DEFINER;

	"SFUNC" return SFUNC;
	"PREFUNC" return PREFUNC;
	"FINALFUNC" return FINALFUNC;
	"SORTOP" return SORTOP;
	"BASETYPE" return BASETYPE;
	"STYPE" return STYPE;
	"INITCOND" return INITCOND;
}

<sFUNC_DECL,sFUNC_ARGLIST,sFUNC_OPTIONS>{
	{IDENTIFIER} { yylval = strdup(yytext); return IDENTIFIER; }
	{INTEGER} { yylval = strdup(yytext); return INTEGER_LITERAL; }
	[^;]|\n return yytext[0];
}

";" { BEGIN(INITIAL); return ';'; }


	/* Contiguity of comment blocks is meaningful and therefore has to be
	 * preserved. Note that input '\n' is handled above */
<sAFTER_COMMENT>. {
	BEGIN(INITIAL);
	return '\n';
}

	/* Default action if nothing else applies: consume next character and do nothing */
.|\n { BEGIN(INITIAL); }

%%

/* C code */

