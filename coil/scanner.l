/*
 * Copyright (C) 2012 John O'Connor
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
%option reentrant
%option noinput
%option nounput
%option noyywrap
%option bison-locations
%option never-interactive

%{
#define YY_USER_ACTION {                                                 \
    yylloc->line = yylineno;                                             \
    yylloc->filepath = (yyextra) ? (gchar *)yyextra->filepath : NULL;    \
}

#include "common.h"
#include "error.h"

#include "parser_defs.h"
#include "parser.h"

static GString *
copy_compressed_string(const gchar *str, guint len)
{
    GString *buffer;
    const gchar *s;
    guint n;

    if (len == 0) {
        return g_string_sized_new(2);
    }

    buffer = g_string_sized_new(len + 1);
    for (s = str, n = len; n > 0; s++, n--) {
        if (*s == '\\') {
            s++; n--;
            switch (*s) {
                case '\0':
                    g_warning("%s: trailing \\", G_STRLOC);
                    return buffer;
                case '$':
                    g_string_append_len(buffer, "\\$", 2);
                    break;
                case 'n':
                    g_string_append_c(buffer, '\n');
                    break;
                case 'r':
                    g_string_append_c(buffer, '\r');
                    break;
                case 't':
                    g_string_append_c(buffer, '\t');
                    break;
                default:
                    g_string_append_c(buffer, *s);
                    break;
            }
        }
        else {
            g_string_append_c(buffer, *s);
        }
    }
    return buffer;
}

#define REMOVE_STRING_TOKEN_QUOTES(s, len)                                   \
G_STMT_START {                                                               \
    if (len > 1) {                                                           \
        const gchar *e = (s + len) - 1;                                      \
        while (s < e && *e == *s && (*s == '\'' || *s == '"' )) {            \
            s++; e--;                                                        \
        }                                                                    \
        len = (e - s) + 1;                                                   \
    }                                                                        \
} G_STMT_END

/* TODO(jcon): add line numbers */
#define HANDLE_PATH_TOKEN()                                                  \
G_STMT_START {                                                               \
    /* tokenizer will only create valid paths except for length */           \
    if (yyleng > COIL_PATH_LEN) {                                            \
        path_length_error(yytext, yyleng);                                   \
        return ERROR;                                                        \
    }                                                                        \
    yylval->path = coil_path_new_len(yytext, yyleng);                        \
} G_STMT_END

%}

/* basic tokens */
D [0-9]
DOUBLE "-"?(({D}+\.{D}*)|({D}*\.{D}+))
INTEGER "-"?{D}+
COMMENT "#"[^\n]*

TRUE (?i:"true")
FALSE (?i:"false")
NONE (?i:"none")

INCLUDE (?i:(("@file"|"@include")[s]?))
EXTEND (?i:"@extend"[s]?)
PACKAGE (?i:"@package")

/* keys and paths */
ROOT "@root"

KEY "-"*[a-zA-Z\_][a-zA-Z0-9_\-]*
KEY_DELIM "."

ROOT_PATH {ROOT}
ABSOLUTE_PATH {ROOT}({KEY_DELIM}{KEY})*
RELATIVE_PATH {KEY_DELIM}?{KEY}({KEY_DELIM}{KEY})*
REFERENCE_PATH {KEY_DELIM}{2,}{RELATIVE_PATH}
PATH ({ABSOLUTE_PATH}|{RELATIVE_PATH}|{REFERENCE_PATH})

/* single and multiline string literals */
ESC_CHR "\\".
NEWLINE ("\r"|"\n"|"\r\n")
WHITESPACE [:space:]

DQ_CHR ({ESC_CHR}|[^\\"])
SQ_CHR ({ESC_CHR}|[^\\\'])

MULTILINE_STRING0 [']{3}({SQ_CHR}|[']{1,2}[^'])*[']{3}
MULTILINE_STRING1 ["]{3}({DQ_CHR}|["]{1,2}[^\"])*["]{3}
MULTILINE_STRING ({MULTILINE_STRING0}|{MULTILINE_STRING1})

SQ_STRING [']{SQ_CHR}*[']
DQ_STRING ["]{DQ_CHR}*["]

STRING_LITERAL ({SQ_STRING}|{DQ_STRING}|{MULTILINE_STRING})

/* expressions */
EXPR_VAR "${"{PATH}"}"

EXPR_DQ_CHR ({ESC_CHR}|[^\\"$])
EXPR_SQ_CHR ({ESC_CHR}|[^\\'$])

/* single line expressions */
EXPR0 ["]{EXPR_DQ_CHR}*{EXPR_VAR}({EXPR_DQ_CHR}|{EXPR_VAR})*["]
EXPR1 [']{EXPR_SQ_CHR}*{EXPR_VAR}({EXPR_SQ_CHR}|{EXPR_VAR})*[']

/* multiline expressions */
EXPR_DQ_ML_CHR ({EXPR_DQ_CHR}|["]{1,2}[^"\\$])
EXPR_SQ_ML_CHR ({EXPR_DQ_CHR}|[']{1,2}[^'\\$])

EXPR2 ["]{3}{EXPR_DQ_ML_CHR}*{EXPR_VAR}({EXPR_DQ_ML_CHR}|{EXPR_VAR})*["]{3}
EXPR3 [']{3}{EXPR_SQ_ML_CHR}*{EXPR_VAR}({EXPR_SQ_ML_CHR}|{EXPR_VAR})*[']{3}

STRING_EXPRESSION ({EXPR0}|{EXPR1}|{EXPR2}|{EXPR3})
%%

{COMMENT} { ; }

{PACKAGE} { return PACKAGE_SYM; }

{INCLUDE} { return INCLUDE_SYM; }

{EXTEND} { return EXTEND_SYM; }

{NONE} { return NONE_SYM; }

{TRUE} { return TRUE_SYM; }

{FALSE} { return FALSE_SYM; }

{ROOT_PATH} {
    HANDLE_PATH_TOKEN();
    return ROOT_PATH;
}

{ABSOLUTE_PATH} {
    HANDLE_PATH_TOKEN();
    return ABSOLUTE_PATH;
}

{RELATIVE_PATH} {
    HANDLE_PATH_TOKEN();
    return RELATIVE_PATH;
}

{REFERENCE_PATH} {
    HANDLE_PATH_TOKEN();
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
    const gchar *str = yytext;
    guint        len = yyleng;

    REMOVE_STRING_TOKEN_QUOTES(str, len);
    yylval->gstring = copy_compressed_string(str, len);

    return STRING_EXPRESSION;
}

{STRING_LITERAL} {
    const gchar *str = yytext;
    guint        len = yyleng;

    REMOVE_STRING_TOKEN_QUOTES(str, len);
    yylval->gstring = copy_compressed_string(str, len);

    return STRING_LITERAL;
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
