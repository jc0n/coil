/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */
#ifndef __COIL_VALUE_H
#define __COIL_VALUE_H

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

typedef enum {

  LEGACY                     = 1 << 0,
  COMPACT                    = 1 << 1,

  FLATTEN_PATHS              = 1 << 2,

  ESCAPE_QUOTES              = 1 << 3,

  BLANK_LINE_AFTER_COMMA     = 1 << 4,
  BLANK_LINE_AFTER_BRACE     = 1 << 5,
  BLANK_LINE_AFTER_STRUCT    = 1 << 6,
  COMMAS_IN_LIST             = 1 << 7,
  BRACE_ON_PATH_LINE         = 1 << 8,

  DONT_EXPAND                = 1 << 9,
  DONT_QUOTE_STRINGS         = 1 << 10,

} CoilStringFormatOptions;


typedef struct _CoilStringFormat
{
  CoilStringFormatOptions options;

  guint                   block_indent;
  guint                   brace_indent;

  guint                   multiline_len;

  guint                   indent_level;
} CoilStringFormat;

extern CoilStringFormat default_string_format;

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

#define new_value(dst, type, v_func, ptr)                         \
        G_STMT_START                                              \
        {                                                         \
          dst = g_slice_new0(GValue);                             \
          g_value_init(dst, type);                                \
          G_PASTE_ARGS(g_value_,v_func)(dst, ptr);                \
        }                                                         \
        G_STMT_END

G_BEGIN_DECLS

GType
coil_none_get_type(void) G_GNUC_CONST;

/* TODO(jcon): namespace the following non-namespaced fn's */
GValue *
value_alloc(void);

GValue *
copy_value(const GValue *value);

GList *
copy_value_list(const GList *value_list);

void
free_value(gpointer value);

void
free_value_list(GList *list);

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
