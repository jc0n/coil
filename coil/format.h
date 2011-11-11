/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */

#ifndef __COIL_FORMAT_H
#define __COIL_FORMAT_H

typedef struct _CoilStringFormat CoilStringFormat;

#include "object.h"

typedef enum
{
    LEGACY                     = 1 << 0,
    COMPACT                    = 1 << 1,

    FLATTEN_PATHS              = 1 << 2,

    ESCAPE_QUOTES              = 1 << 3,

    BLANK_LINE_AFTER_ITEM      = 1 << 4,
    BLANK_LINE_AFTER_BRACE     = 1 << 5,
    BLANK_LINE_AFTER_STRUCT    = 1 << 6,

    COMMAS_IN_LIST             = 1 << 7,

    BRACE_ON_BLANK_LINE        = 1 << 8,
    LIST_ON_BLANK_LINE         = 1 << 9,

    FORCE_EXPAND               = 1 << 10,
    DONT_QUOTE_STRINGS         = 1 << 11,
} CoilStringFormatOptions;

struct _CoilStringFormat
{
    CoilStringFormatOptions options;

    guint8 block_indent;
    guint8 brace_indent;
    guint multiline_len;
    guint indent_level;

    CoilObject *context;
};

extern CoilStringFormat default_string_format;

#endif
