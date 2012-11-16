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
#ifndef __COIL_VALUE_H
#define __COIL_VALUE_H

#include "format.h"

typedef struct _CoilNone      CoilNone;
typedef struct _CoilNoneClass CoilNoneClass;

#define COIL_TYPE_NONE (coil_none_get_type())
extern CoilNone *coil_none_object;

struct _CoilNone
{
    GObject parent_instance;
};

struct _CoilNoneClass
{
    GObjectClass parent_class;
};

typedef GValue CoilValue;

/* block padding chars for string output */
#define COIL_BLOCK_PADDING "    " /* 4 spaces */
#define COIL_BLOCK_PADDING_LEN                                               \
  (COIL_STATIC_STRLEN(COIL_BLOCK_PADDING))

/* character to quote single line strings */
#define COIL_STRING_QUOTE '\''
/* string escape character */
#define COIL_STRING_ESCAPE '\\'
/* multiline quote string */
#define COIL_MULTILINE_QUOTE "'''"
/* multiline quotes after line exceeds n chars */
#define COIL_MULTILINE_LEN 80

#define coil_value_init(v_ptr, type, v_func, ptr)                            \
G_STMT_START                                                                 \
{                                                                            \
    v_ptr = coil_value_alloc();                                              \
    g_value_init(v_ptr, type);                                               \
    G_PASTE_ARGS(g_value_,v_func)(v_ptr, ptr);                               \
}                                                                            \
G_STMT_END

G_BEGIN_DECLS

GType
coil_none_get_type(void) G_GNUC_CONST;

CoilValue *
coil_value_alloc(void);

CoilValue *
coil_value_copy(const CoilValue *value);

GList *
coil_value_list_copy(const GList *value_list);

void
coil_value_free(gpointer value);

void
coil_value_list_free(GList *list);

void
free_string_list(GList *list);

void
coil_value_build_string(const CoilValue *value, GString *const buffer,
        CoilStringFormat *format);

gchar *
coil_value_to_string(const CoilValue *value, CoilStringFormat *format);

gint
coil_value_compare(const CoilValue *v1, const CoilValue *v2);

gboolean
coil_value_equals(const CoilValue *v1, const CoilValue *v2);

G_END_DECLS

#endif
