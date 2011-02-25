/*
 * Copyright (C) 2009, 2010
 *
 * Author: John O'Connor
 */
%option reentrant
%option noyywrap
%option bison-bridge
%option bison-locations

%{
#define YY_USER_ACTION                                          \
  {                                                             \
    register int i = yyleng;                                    \
    yylloc->first_line = yylloc->last_line = yylineno;          \
    yylloc->filepath = (gchar *)yyextra->filepath;              \
    while (i-- > 0)                                             \
      switch (yytext[i])                                        \
      {                                                         \
        case '\n':                                              \
          yylloc->last_line++;                                  \
          yylloc->last_column = 0;                              \
          break;                                                \
        case '\t':                                              \
          yylloc->last_column += 8 - (yylloc->last_column % 8); \
          break;                                                \
        default:                                                \
          yylloc->last_column++;                                \
      }                                                         \
  }

#include "common.h"
#include "error.h"

#include "parser_defs.h"
#include "parser.h"
%}

D [0-9]
DOUBLE "-"?(({D}+\.{D}*)|({D}*\.{D}+))
INTEGER "-"?{D}+
COMMENT "#"[^\n]*

ROOT "@root"

KEY "-"*[a-zA-Z\_][a-zA-Z0-9_\-]*
KEY_DELIM "."

ROOT_PATH {ROOT}
ABSOLUTE_PATH {ROOT}({KEY_DELIM}{KEY})*
RELATIVE_PATH {KEY_DELIM}?{KEY}({KEY_DELIM}{KEY})*
REFERENCE_PATH {KEY_DELIM}{2,}{RELATIVE_PATH}
PATH ({ABSOLUTE_PATH}|{RELATIVE_PATH}|{REFERENCE_PATH})

ESC "\\".
NEWLINE ("\r"|"\n"|"\r\n")
WHITESPACE [:space:]

DQ_CHR {ESC}|[^\\"]
SQ_CHR {ESC}|[^\\\']

MULTILINE_STRING0 [']{3}({SQ_CHR}|[']{1,2}[^'])*[']{3}
MULTILINE_STRING1 ["]{3}({DQ_CHR}|["]{1,2}[^\"])*["]{3}
MULTILINE_STRING ({MULTILINE_STRING0}|{MULTILINE_STRING1})

SQ_STRING [']{SQ_CHR}*[']
DQ_STRING ["]{DQ_CHR}*["]
STRING ({SQ_STRING}|{DQ_STRING})

EXPR_CHR ({ESC}|[^\\"$])
EXPR_VAR "${"{PATH}"}"
STRING_EXPRESSION ["]{EXPR_CHR}*{EXPR_VAR}({EXPR_CHR}|{EXPR_VAR})*["]

MODULE ("@"[a-zA-Z]+)

TRUE (?i:"true")
FALSE (?i:"false")
NONE (?i:"none")

INCLUDE (?i:(("@file"|"@include")[s]?))
EXTEND (?i:"@extend"[s]?)
PACKAGE (?i:"@package")
LINK (?i:"link")

%%

{COMMENT} { ; }

{PACKAGE} { return PACKAGE_SYM; }

{INCLUDE} { return INCLUDE_SYM; }

{EXTEND} { return EXTEND_SYM; }

{LINK}/"(" { return LINK_SYM; }

{NONE} { return NONE_SYM; }

{TRUE} { return TRUE_SYM; }

{FALSE} { return FALSE_SYM; }

{ROOT_PATH} {
  yylval->path = coil_path_new_len(yytext, yyleng);
  return ROOT_PATH;
}

{ABSOLUTE_PATH} {
  yylval->path = coil_path_new_len(yytext, yyleng);
  return ABSOLUTE_PATH;
}

{RELATIVE_PATH} {
  yylval->path = coil_path_new_len(yytext, yyleng);
  return RELATIVE_PATH;
}

{REFERENCE_PATH} {
  yylval->path = coil_path_new_len(yytext, yyleng);
  return REFERENCE_PATH;
}

{DOUBLE} {
  yylval->doubleval = g_ascii_strtod(yytext, NULL);
  return DOUBLE;
}

{INTEGER} {
  yylval->longint = strtol(yytext, NULL, 10);
  return INTEGER;
}

{STRING_EXPRESSION} {
  yylval->gstring = g_string_new_len(yytext + 1, yyleng - 2);
  return STRING_EXPRESSION;
}

{MULTILINE_STRING} {
  yylval->gstring = g_string_new_len(yytext + 3, yyleng - 6);
  return STRING_LITERAL;
}

{STRING} {
  yylval->gstring = g_string_new_len(yytext + 1, yyleng - 2);
  return STRING_LITERAL;
}

{MODULE} {
  // to be implemented later
  /* TODO: check that module exists in module table */
  if (/*XXX: found module in lookup table */FALSE)
  {
    yylval->gstring = g_string_new_len(yytext + 1, yyleng - 2);
    return MODULE_SYM;
  }
  else
    yymore();
}

"~" { return '~'; }
":" { return ':'; }
"@" { return '@'; }

"(" { return '('; }
")" { return ')'; }

"{" { return '{'; }
"}" { return '}'; }

"[" { return '['; }
"]" { return ']'; }
"," { return ','; }
"=" { return '='; }

[\n]    { yylineno++; }
[\t\r ] { ; }

. { return yytext[0]; }

%%
