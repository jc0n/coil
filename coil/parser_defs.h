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
#ifndef _COIL_PARSER_EXTRAS_H
#define _COIL_PARSER_EXTRAS_H

#include <stdio.h>

#include "error.h"
#include "struct.h"
#include "list.h"

#define YYLTYPE CoilLocation
#define YYLTYPE_IS_DECLARED 1
#define YYLTYPE_IS_TRIVIAL 0

#define YYLLOC_DEFAULT(Current, Rhs, N)                                      \
    do {                                                                     \
        if (YYID(N)) {                                                       \
            (Current).line = YYRHSLOC(Rhs, 1).line;                          \
            (Current).filepath = YYRHSLOC(Rhs, N).filepath;                  \
        }                                                                    \
        else {                                                               \
            (Current).line = YYRHSLOC(Rhs, 0).line;                          \
            (Current).filepath =  NULL;                                      \
        }                                                                    \
    } while (0)

#define YY_EXTRA_TYPE CoilParser *
#define YYPARSE_PARAM yyctx
#define YYCTX ((YY_EXTRA_TYPE)YYPARSE_PARAM)
#define YYLEX_PARAM YYCTX->scanner

typedef struct _CoilParser
{
    gchar       *filepath;
    CoilObject  *root;
    CoilPath    *path;
    gulong       prototype_hook_id;
    GQueue       containers;
    GHashTable  *prototypes;
    GList       *errors;
    gpointer     scanner;
    gpointer     buffer_state;
    gboolean     do_buffer_gc : 1;
} CoilParser;

G_BEGIN_DECLS

COIL_API(CoilObject *)
coil_parse_stream(FILE *, const gchar *);

COIL_API(CoilObject *)
coil_parse_file(const gchar *filepath);

COIL_API(CoilObject *)
coil_parse_string(const gchar *string);

COIL_API(CoilObject *)
coil_parse_string_len(const gchar *string, gsize len);

COIL_API(CoilObject *)
coil_parse_buffer(gchar *buffer, gsize len);

G_END_DECLS
#endif

