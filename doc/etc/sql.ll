/* -----------------------------------------------------------------------------
 * sql.ll
 *
 * A simple flex file for use in companion with sql.yy. Use case: Documenting
 * .sql files with tools like Doxygen.
 * 
 * Revision History:
 * 0.2: Florian Schoppmann, 16 Jan 2011, Converted to C++
 * 0.1:          "        , 10 Jan 2011, Initial version.
 * -----------------------------------------------------------------------------
 */

/* Definitions */

/* Use C++ */
%option c++

%option never-interactive

/* change the name of the scanner class. results in "SQLFlexLexer" */
%option prefix="SQL"

/* Generate a "case-insensitive" scanner. The case of letters given in the
 * `flex' input patterns will be ignored, and tokens in the input will be
 * matched regardless of case */
%option case-insensitive

/* makes the scanner not call `yywrap()' upon an end-of-file, but simply assume
 * that there are no more files to scan */
%option noyywrap

/* instructs `flex' to write the scanner it generates to standard output instead
 * of `lex.yy.c' */
%option stdout


/* C++ Code */
%{
	/* We define COMPILING_SCANNER in order to know in sql.parser.hh whether we are compiling
	 * the parser or the scanner */
	#define COMPILING_SCANNER 1
	
	#include "sql.parser.hh"

	#include <string>	

	/* import the parser's token type into a local typedef */
	typedef bison::SQLParser::token	token;

	int		stringCaller;
	char	*stringLiteralQuotation = NULL;

	/* yylloc is declared by macro YY_DECL */
	#define YY_USER_ACTION preScannerAction(yylval, yylloc, driver);

%}

/* Definitions */
CREATE_FUNCTION "CREATE"{SPACE}("OR"{SPACE}"REPLACE"{SPACE})?"FUNCTION"
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
	*yylval = static_cast<char *>( malloc(yyleng - 1) );
	strcpy(*yylval, yytext + 2);
	BEGIN(sAFTER_COMMENT);
	
	/* consume the newline character (we thus need to advance the line count) */
	yyinput();
	yylloc->lines(1);
	
	return token::COMMENT;
}

	/* String literals in single quotes */
"'" { stringCaller = YY_START; BEGIN(sSTRING_LITERAL); }

<sSTRING_LITERAL>{
	"''" { yymore(); }
	"\\'" { yymore(); }
	"'" {
		yytext[yyleng - 1] = '\0';
		*yylval = strdup(yytext);
		BEGIN(stringCaller);
		if (stringCaller != INITIAL && stringCaller != sAFTER_COMMENT)
			return token::STRING_LITERAL;
	}
	. { yymore(); }
}

	/* String literals in dollar quotes, see
	http://www.postgresql.org/docs/current/static/sql-syntax-lexical.html#SQL-SYNTAX-DOLLAR-QUOTING */
{DOLLARQUOTE} {
	stringCaller = YY_START;
	stringLiteralQuotation = static_cast<char *>( malloc(yyleng - 1) );
	strncpy(stringLiteralQuotation, yytext + 1, yyleng - 1);
	BEGIN(sDOLLAR_STRING_LITERAL);
}

<sDOLLAR_STRING_LITERAL>{
	{DOLLARQUOTE} {
		if (strncmp(yytext + 1, stringLiteralQuotation, yyleng - 1) == 0) {
			*yylval = "<omitted by lexer>";
			BEGIN(stringCaller);
			free(stringLiteralQuotation);
			stringLiteralQuotation = NULL;
			if (stringCaller != INITIAL && stringCaller != sAFTER_COMMENT)
				return token::STRING_LITERAL;
		} else {
			yymore();
		}
	}
	.|\n
}


{CREATE_FUNCTION} { BEGIN(sFUNC_DECL); return token::CREATE_FUNCTION; }

<sFUNC_DECL>"(" { BEGIN(sFUNC_ARGLIST); return '('; }
<sFUNC_ARGLIST>")" { BEGIN(sFUNC_OPTIONS); return ')'; }

	/* We disallow using the following keywords as argument names */
<sFUNC_ARGLIST,sFUNC_OPTIONS>{
	"IN" return token::IN;
	"OUT" return token::OUT;
	"INOUT" return token::INOUT;
	
	"BIT" return token::BIT;
	"CHARACTER" return token::CHARACTER;
	"DOUBLE" return token::DOUBLE;
	"PRECISION" return token::PRECISION;
	"TIME" return token::TIME;
	"WITH" return token::WITH;
	"WITHOUT" return token::WITHOUT;
	"VOID" return token::VOID;
	"VARYING" return token::VARYING;
	"ZONE" return token::ZONE;

	"RETURNS" return token::RETURNS;
	"SETOF" return token::SETOF;
	
	"AS" return token::AS;
	"LANGUAGE" return token::LANGUAGE;
	"IMMUTABLE" return token::IMMUTABLE;
	"STABLE" return token::STABLE;
	"VOLATILE" return token::VOLATILE;
	"CALLED"{SPACE}"ON"{SPACE}"NULL"{SPACE}"INPUT" return token::CALLED_ON_NULL_INPUT;
	"RETURNS"{SPACE}"NULL"{SPACE}"ON"{SPACE}"NULL"{SPACE}"INPUT"|"STRICT" {
		return token::RETURNS_NULL_ON_NULL_INPUT; }
	("EXTERNAL"{SPACE})?"SECURITY"{SPACE}"INVOKER" return token::SECURITY_INVOKER;
	("EXTERNAL"{SPACE})?"SECURITY"{SPACE}"DEFINER" return token::SECURITY_DEFINER;
}

<sFUNC_DECL,sFUNC_ARGLIST,sFUNC_OPTIONS>{
	{IDENTIFIER} { *yylval = strdup(yytext); return token::IDENTIFIER; }
	{INTEGER} { *yylval = strdup(yytext); return token::INTEGER_LITERAL; }
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

/* C++ code */

namespace bison {

/* The class declaration of SQLScanner is in sql.yy (because bison generates
 * the header file). */

SQLScanner::SQLScanner(std::istream *arg_yyin, std::ostream *arg_yyout) :
	SQLFlexLexer(arg_yyin, arg_yyout) {
	/* only has an effect if %option debug or flex -d is used */
	set_debug(1);
}

SQLScanner::~SQLScanner() {
}

void SQLScanner::preScannerAction(SQLParser::semantic_type *yylval,
	SQLParser::location_type *yylloc, SQLDriver *driver) {
	
	for (int i = 0; i < yyleng; i++) {
		if (yytext[i] == '\r' && i < yyleng - 1 && yytext[i + 1] == '\n') {
			i++; yylloc->lines(1);
		} else if (yytext[i] == '\r' || yytext[i] == '\n') {
			yylloc->lines(1);
		} else { \
			yylloc->columns(1);
		}
	}
	yylloc->step(); \
}

} // namespace bison
