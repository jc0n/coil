/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */

#include "common.h"

#include <string.h>

#include "list.h"
#include "value.h"

/*
 * coil_list_get_type:
 *
 * Return the type identifier for CoilList
 */
GType
coil_list_get_type(void)
{
  static GType type_id = 0;

  if (!type_id)
    type_id = g_boxed_type_register_static(g_intern_static_string("CoilList"),
                                           (GBoxedCopyFunc)g_list_copy,
                                           (GBoxedFreeFunc)g_list_free);

  return type_id;
}

/*
 * coil_list_build_string:
 * @list: out #GList
 * @buffer: pre-allocated #GString buffer
 * @error: #GError reference or NULL.
 *
 * Build a string representation for list in the
 * specified buffer.
 *
 */
COIL_API(void)
coil_list_build_string(const GList      *list,
                       GString          *const buffer,
                       CoilStringFormat *format,
                       GError          **error)
{
  g_return_if_fail(buffer);
  g_return_if_fail(format);
  g_return_if_fail(error == NULL || *error == NULL);

  GError       *internal_error = NULL;
  gchar         delim[128];
  guint8        dlen = 1;
  guint         orig_indent_level = format->indent_level;
  gboolean      whitespace = !(format->options & COMPACT);
  const GValue *value;

  if (list == NULL)
  {
    g_string_append_len(buffer, COIL_STATIC_STRLEN("[]"));
    return;
  }

  memset(delim, 0, sizeof(delim));

  if (format->options & COMMAS_IN_LIST)
  {
    delim[0] = ',';

    if (format->options & BLANK_LINE_AFTER_COMMA)
    {
      format->indent_level += format->block_indent;
      delim[dlen++] = '\n';
      memset(delim + dlen, ' ',
             MIN(format->indent_level, sizeof(delim)));
      dlen += format->indent_level;
    }
    else if (whitespace)
      delim[dlen++] = ' ';
  }
  else
    delim[0] = ' ';

  g_string_append_c(buffer, '[');

  do
  {
    value = (GValue *)list->data;
    coil_value_build_string(value, buffer,
                            format, &internal_error);

    if (G_UNLIKELY(internal_error))
    {
      g_propagate_error(error, internal_error);
      return;
    }

    g_string_append_len(buffer, delim, dlen);

  } while ((list = g_list_next(list)));

  g_string_truncate(buffer, buffer->len - dlen);
  g_string_append_c(buffer, ']');

  /* restore indent at this scope */
  format->indent_level = orig_indent_level;
}

/*
 * coil_list_to_string:
 * @list: our #GList
 * @error: #GError reference
 *
 * Create a string representation from a list.
 */
COIL_API(gchar *)
coil_list_to_string(const GList      *list,
                    CoilStringFormat *format,
                    GError          **error)
{
  GString *buffer;

  g_return_val_if_fail(error == NULL || *error == NULL, NULL);

  if (list == NULL)
    return g_strndup(COIL_STATIC_STRLEN("[]"));

  buffer = g_string_sized_new(128);
  coil_list_build_string(list, buffer, format, error);

  return g_string_free(buffer, FALSE);
}

