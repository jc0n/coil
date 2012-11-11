/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */
#ifndef _COIL_PARSER_EXTRAS_H
#define _COIL_PARSER_EXTRAS_H

#include <stdio.h>

#include "error.h"
#include "struct.h"

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

