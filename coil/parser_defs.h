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
#define YYLTYPE_IS_TRIVIAL 1

#define YYLLOC_DEFAULT(Current, Rhs, N)                                \
    do                                                                 \
      if (YYID(N))                                                     \
        {                                                              \
          (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;       \
          (Current).first_column = YYRHSLOC (Rhs, 1).first_column;     \
          (Current).last_line    = YYRHSLOC (Rhs, N).last_line;        \
          (Current).last_column  = YYRHSLOC (Rhs, N).last_column;      \
          (Current).filepath     = YYRHSLOC (Rhs, N).filepath;         \
        }                                                              \
      else                                                             \
        {                                                              \
          (Current).first_line   = (Current).last_line   =             \
            YYRHSLOC (Rhs, 0).last_line;                               \
          (Current).first_column = (Current).last_column =             \
            YYRHSLOC (Rhs, 0).last_column;                             \
          (Current).filepath     =  NULL;                              \
        }                                                              \
    while (0)

#define YY_EXTRA_TYPE CoilParser *
#define YYPARSE_PARAM yyctx
#define YYCTX ((YY_EXTRA_TYPE)YYPARSE_PARAM)

#define YYLEX_PARAM YYCTX->scanner

typedef struct _CoilParser
{
    const gchar *filepath;
    CoilObject  *root;
    CoilPath    *path;
    gulong       prototype_hook_id;
    GQueue       containers;
    GHashTable  *prototypes;
    /*XXX: rename to last_error?? */
    GError      *error;
    GList       *errors;
    gpointer     scanner;
    gpointer     buffer_state;
    gboolean     do_buffer_gc : 1;
} CoilParser;

COIL_API(CoilObject *)
coil_parse_stream(FILE *, const gchar *, GError **err);

COIL_API(CoilObject *)
coil_parse_file(const gchar *filepath, GError **err);

COIL_API(CoilObject *)
coil_parse_string(const gchar *string, GError **err);

COIL_API(CoilObject *)
coil_parse_string_len(const gchar *string, gsize len, GError **err);

COIL_API(CoilObject *)
coil_parse_buffer(gchar *buffer, gsize len, GError **error);

#endif

