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

/* instructs flex to generate a batch scanner, the opposite of interactive
 * scanners */
%option batch

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

	/* FIXME: Make class variables */
	int		stringCaller;
	char	*stringLiteralQuotation = NULL;
	
	/* YY_USER_ACTION is called from the lex() function, which has the signature
	 * and name as defined by macro YY_DECL. yylval, yylloc, and driver are
	 * arguments. */
	#define YY_USER_ACTION preScannerAction(yylval, yylloc, driver);
%}

/* Definitions */
CREATE_FUNCTION "CREATE"{SPACE}("OR"{SPACE}"REPLACE"{SPACE})?"FUNCTION"
CREATE_AGGREGATE "CREATE"{SPACE}"AGGREGATE"
COMMENT "--".*$
CCOMMENT "/*"([^\*]|\*[^/])*"*/"
IDENTIFIER [[:alpha:]_][[:alnum:]_]*
QUOTED_IDENTIFIER "\""{IDENTIFIER}"\""
COMMENTED_IDENTIFIER "/*"[[:space:]]*({IDENTIFIER}|{QUOTED_IDENTIFIER})[[:space:]]*"*/"
INTEGER [[:digit:]]+
SPACE [[:space:]]+
DOLLARQUOTE "$$"|"$"{IDENTIFIER}"$"


/* State definitions */

%s sAFTER_COMMENT
%s sFUNC_DECL
%s sFUNC_ARGLIST
%s sFUNC_OPTIONS
%s sAGG_DECL
%s sAGG_ARGLIST
%s sAGG_OPTIONS
%x sSTRING_LITERAL
%x sDOLLAR_STRING_LITERAL



%%	/* Rules */

	/* Contiguity of comment blocks is meaningful and therefore has to be
	 * preserved. Note that input . is handled below */
<sAFTER_COMMENT>[[:space:]]*\n[[:space:]]* {
	BEGIN(INITIAL);
	return '\n';
}

	/* Ignore spaces */
{SPACE}

{COMMENT} {
	yytext[0] = yytext[1] = '/';
	yylval->str = static_cast<char *>( malloc(yyleng + 2) );
	strcpy(yylval->str, yytext);
	yylval->str[yyleng] = '\n'; 
	yylval->str[yyleng + 1] = 0; 
	BEGIN(sAFTER_COMMENT);
	
	/* consume the newline character (we thus need to advance the line count) */
	yyinput();
	yylloc->lines(1);
	
	return token::COMMENT;
}

    /* Since PostgreSQL (the SQL standard?) disallows labeling arguments of
     * aggregate functions, we will treat a C-style comment consisting of a
     * single word as the argument label. */
    /* We consume whitespace and newline after the comment only because the next
     * rule does so, too -- we need to be the longest applicable rule. */
<sAGG_ARGLIST>{COMMENTED_IDENTIFIER}([[:space:]]*\n[[:space:]]*)? {
    int     startpos = 2, endpos = yyleng - 3;
    bool    quoted = false;
    
    while (isspace(yytext[startpos])) startpos++;
    while (isspace(yytext[endpos])) endpos--;
    if (yytext[startpos] == '\"') {
        // QUOTED_IDENTIFIER
        startpos++; endpos--;
        quoted = true;
    }
    yytext[endpos + 1] = 0;
    yylval->str = quoted ? strdup(yytext + startpos) : strlowerdup(yytext + startpos);
    return token::IDENTIFIER;
}

	/* If only whitespace and a newline follow, also consume that */
{CCOMMENT}([[:space:]]*\n[[:space:]]*)? {
	yylval->str = strdup(yytext);
	BEGIN(sAFTER_COMMENT);
	return token::COMMENT;
}

	/* String literals in single quotes */
"'" { stringCaller = YY_START; BEGIN(sSTRING_LITERAL); }

<sSTRING_LITERAL>{
	"''" { yymore(); }
	"\\'" { yymore(); }
	"'" {
		yytext[yyleng - 1] = '\0';
		yylval->str = strdup(yytext);
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
			yylval->str = "<omitted by lexer>";
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

{CREATE_AGGREGATE} { BEGIN(sAGG_DECL); return token::CREATE_AGGREGATE; }

<sFUNC_DECL,sAGG_DECL>"(" {
	if (YY_START == sFUNC_DECL)
		BEGIN(sFUNC_ARGLIST);
	else
		BEGIN(sAGG_ARGLIST);

	return '(';
}
<sFUNC_ARGLIST,sAGG_ARGLIST>")" {
	if (YY_START == sFUNC_ARGLIST)
		BEGIN(sFUNC_OPTIONS);
	else
		BEGIN(sAGG_OPTIONS);

	return ')';
}

	/* We disallow using the following keywords as argument names */
<sFUNC_ARGLIST,sFUNC_OPTIONS,sAGG_ARGLIST,sAGG_OPTIONS>{
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

	/* We disallow using the following keywords as argument names */
<sAGG_ARGLIST,sAGG_OPTIONS>{
	"SFUNC" return token::SFUNC;
	"PREFUNC" return token::PREFUNC;
	"FINALFUNC" return token::FINALFUNC;
	"STYPE" return token::STYPE;
	"INITCOND" return token::INITCOND;
	"SORTOP" return token::SORTOP;
}

<sFUNC_DECL,sFUNC_ARGLIST,sFUNC_OPTIONS,sAGG_DECL,sAGG_ARGLIST,sAGG_OPTIONS>{
	{QUOTED_IDENTIFIER} {
		yytext[yyleng - 1] = 0;
		yylval->str = strdup(yytext + 1);
		return token::IDENTIFIER;
	}
	{IDENTIFIER} { yylval->str = strlowerdup(yytext); return token::IDENTIFIER; }
	{INTEGER} { yylval->str = strdup(yytext); return token::INTEGER_LITERAL; }
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

char *SQLScanner::strlowerdup(const char *inString) {
	char *returnStr = strdup(inString);
	for (int i = 0; returnStr[i]; i++)
		returnStr[i] = tolower(returnStr[i]);
	return returnStr;
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
