/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
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

/* block padding chars for string output */
#define COIL_BLOCK_PADDING "    " /* 4 spaces */
#define COIL_BLOCK_PADDING_LEN                                      \
  (COIL_STATIC_STRLEN(COIL_BLOCK_PADDING))

/* character to quote single line strings */
#define COIL_STRING_QUOTE '\''
/* string escape character */
#define COIL_STRING_ESCAPE '\\'
/* multiline quote string */
#define COIL_MULTILINE_QUOTE "'''"
/* multiline quotes after line exceeds n chars */
#define COIL_MULTILINE_LEN 80

#define coil_value_init(v_ptr, type, v_func, ptr)           \
  G_STMT_START                                              \
  {                                                         \
    v_ptr = coil_value_alloc();                             \
    g_value_init(v_ptr, type);                              \
    G_PASTE_ARGS(g_value_,v_func)(v_ptr, ptr);              \
  }                                                         \
  G_STMT_END

G_BEGIN_DECLS

GType
coil_none_get_type(void) G_GNUC_CONST;

GValue *
coil_value_alloc(void);

GValue *
coil_value_copy(const GValue *value);

GList *
coil_value_list_copy(const GList *value_list);

void
coil_value_free(gpointer value);

void
coil_value_list_free(GList *list);

void
free_string_list(GList *list);

void
coil_value_build_string(const GValue     *value,
                        GString          *const buffer,
                        CoilStringFormat *format,
                        GError          **error);

gchar *
coil_value_to_string(const GValue     *value,
                     CoilStringFormat *format,
                     GError          **error);

gint
coil_value_compare(const GValue *,
                   const GValue *,
                   GError      **);

G_END_DECLS

#endif
