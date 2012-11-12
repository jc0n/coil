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
