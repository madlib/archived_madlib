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

/* We really use yymore, but only in more(). We need to provide this option
 * because flex with otherwise complain:
 * "error: 'yymore_used_but_not_detected' was not declared in this scope */
%option yymore

/* instructs flex to generate a scanner which never considers its input
 * interactive. Normally, on each new input file the scanner calls isatty() in
 * an attempt to determine whether the scanner's input source is interactive and
 * thus should be read a character at a time. When this option is used, however,
 * then no such call is made.
 * We declare this option because otherwise flex will generate a redundant
 * declaration of isatty(), which may lead to compile errors. */
%option never-interactive


/* C++ Code */
%{
	/* We define COMPILING_SCANNER in order to know in sql.parser.hh whether we are compiling
	 * the parser or the scanner */
	#define COMPILING_SCANNER 1
	
	#include "sql.parser.hh"

	#include <string>

	/* import the parser's token type into a local typedef */
	typedef bison::SQLParser::token	token;
	
	/* YY_USER_ACTION is called from the lex() function, which has the signature
	 * and name as defined by macro YY_DECL. yylval, yylloc, and driver are
	 * arguments. */
	#define YY_USER_ACTION preScannerAction(yylval, yylloc, driver);
%}

/* Definitions */
CREATE_FUNCTION "CREATE"{SPACE}("OR"{SPACE}"REPLACE"{SPACE})?"FUNCTION"
CREATE_AGGREGATE "CREATE"{SPACE}"AGGREGATE"
COMMENT "--"[^\n\r]*(\n|\r\n?)?
BEGIN_CCOMMENT "/*"
END_CCOMMENT ([^\*]|\*[^/])*"*/"
IDENTIFIER [[:alpha:]_][[:alnum:]_]*
QUOTED_IDENTIFIER "\""{IDENTIFIER}"\""
INTEGER [[:digit:]]+
SPACE [[:space:]]+
DOLLARQUOTE "$$"|"$"{IDENTIFIER}"$"
BEGIN_SPECIAL_COMMENT "/*+"
END_SPECIAL_COMMENT "*/"
STRING_LITERAL "'"([^']|''|\\')*"'"
EXPONENT "e"("+"|"-")?[[:digit:]]+
FLOATING_POINT_LITERAL ([[:digit:]]+"."[[:digit:]]*|"."[[:digit:]]+){EXPONENT}?|[[:digit:]]+{EXPONENT}|"'"("+"|"-")?("NaN"|"Infinity")"'"


/* State definitions */

%s sFUNC_DECL
%s sFUNC_ARGLIST
%s sFUNC_OPTIONS
%s sAGG_DECL
%s sAGG_ARGLIST
%s sAGG_OPTIONS
%x sDOLLAR_STRING_LITERAL
%x sCCOMMENT



%%	/* Rules */

	/* Ignore spaces */
{SPACE}

{COMMENT} {
    /* only return as token if it is a Doxygen comment. Otherwise, ignore it. */
    if (yytext[2] == '!') {
        yytext[0] = yytext[1] = '/';
        yylval->str = static_cast<char *>( strdup(yytext) );
        return token::COMMENT;
    }
}

    /* Since not all of Greenplum and PostgreSQL allow the following
     * - labeling arguments of aggregate functions,
     * - default arguments
     * we will simply uncomment C style comments in argument lists when they
     * begin with BEGIN_SPECIAL_COMMENT. */
<sFUNC_ARGLIST,sAGG_ARGLIST>{
    {BEGIN_SPECIAL_COMMENT} { return token::BEGIN_SPECIAL; }
    {END_SPECIAL_COMMENT} { return token::END_SPECIAL; }
}

    /* A C comment is split up into two parts. The reason is that flex tries to
     * match the longest rule and we want to give "normal" C comments a low
     * precedence according to this rule. */
{BEGIN_CCOMMENT} {
    more();
    yy_push_state(sCCOMMENT);
}

<sCCOMMENT>{END_CCOMMENT} {
    yy_pop_state();

    /* only return as token if it is a Doxygen comment. Otherwise, ignore it. */
    if (yytext[2] == '*' || yytext[2] == '!') {
        yylval->str = strdup(yytext);
        return token::COMMENT;
    }
}

<sDOLLAR_STRING_LITERAL>{
	{DOLLARQUOTE} {
		if (strncmp(yytext + 1, stringLiteralQuotation, yyleng - 1) == 0) {
			yylval->str = "\"<omitted by lexer>\"";
			yy_pop_state();
			free(stringLiteralQuotation);
			stringLiteralQuotation = NULL;
			return token::STRING_LITERAL;
		}
	}
    /* Speed up the lexer by matching large chunks of text if possible */
    [^$]*
    "$"
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
    
    "DEFAULT" return token::DEFAULT;
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
    "NULL" {
        yylval->str = strdup("NULL");
        return token::NULL_KEYWORD;
    }

    {QUOTED_IDENTIFIER} {
		yytext[yyleng - 1] = 0;
		yylval->str = strdup(yytext + 1);
		return token::IDENTIFIER;
	}
	{IDENTIFIER} { yylval->str = strlowerdup(yytext); return token::IDENTIFIER; }
    
	{INTEGER} {
        yylval->str = strdup(yytext);
        return token::INTEGER_LITERAL;
    }
    {FLOATING_POINT_LITERAL} {
        yylval->str = strdup(yytext);
        return token::FLOAT_LITERAL;
    }
    {STRING_LITERAL} {
    	/* String literals in single quotes */
        yytext[0] = yytext[yyleng - 1] = '"';
        yylval->str = strdup(yytext);
        return token::STRING_LITERAL;
    }
    {DOLLARQUOTE} {
        /* String literals in dollar quotes, see
        http://www.postgresql.org/docs/current/static/sql-syntax-lexical.html#SQL-SYNTAX-DOLLAR-QUOTING */
        stringLiteralQuotation = static_cast<char *>( malloc(yyleng - 1) );
        strncpy(stringLiteralQuotation, yytext + 1, yyleng - 1);
        yy_push_state(sDOLLAR_STRING_LITERAL);
    }
    
	[^;] { return yytext[0]; }
}

";" { BEGIN(INITIAL); return ';'; }

	/* Default action if nothing else applies: consume next character and do nothing */
.|\n { BEGIN(INITIAL); }

%%

/* C++ code */

namespace bison {

/* The class declaration of SQLScanner is in sql.yy (because bison generates
 * the header file). */

SQLScanner::SQLScanner(std::istream *arg_yyin, std::ostream *arg_yyout) :
	SQLFlexLexer(arg_yyin, arg_yyout), stringLiteralQuotation(NULL), oldLength(0) {
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

void SQLScanner::preScannerAction(SQLParser::semantic_type * /* yylval */,
	SQLParser::location_type *yylloc, SQLDriver * /* driver */) {
	
	yylloc->step();
    
    // Start at oldLength: We don't want to count preserved text more than once
	for (int i = oldLength; i < yyleng; i++) {
		if (yytext[i] == '\r' && i < yyleng - 1 && yytext[i + 1] == '\n') {
			i++; yylloc->lines(1);
		} else if (yytext[i] == '\r' || yytext[i] == '\n') {
			yylloc->lines(1);
		} else {
			yylloc->columns(1);
		}
	}
    
    // Reset oldLength. more() needs to be called if yytext is to be preserved
    // again
    oldLength = 0;
}

void SQLScanner::more() {
    oldLength = yyleng;
    yymore();
}

} // namespace bison
