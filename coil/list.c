/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */

#include "common.h"

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
                                           (GBoxedFreeFunc)free_value_list);

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
coil_list_build_string(const GList  *list,
                       GString      *const buffer,
                       GError      **error)
{
  GError *internal_error = NULL;

  g_return_if_fail(buffer != NULL);
  g_return_if_fail(error == NULL || *error == NULL);

  if (list == NULL)
  {
    g_string_append_len(buffer, COIL_STATIC_STRLEN("[]"));
    return;
  }

  g_string_append_c(buffer, '[');

  do
  {
    g_assert(list->data);

    coil_value_build_string((GValue *)list->data,
                            buffer,
                            &internal_error);

    g_string_append_c(buffer, ' ');

    if (G_UNLIKELY(internal_error))
    {
      g_propagate_error(error, internal_error);
      return;
    }

  } while ((list = g_list_next(list)));

  g_string_truncate(buffer, buffer->len - 1);
  g_string_append_c(buffer, ']');
}

/*
 * coil_list_to_string:
 * @list: our #GList
 * @error: #GError reference
 *
 * Create a string representation from a list.
 */
COIL_API(gchar *)
coil_list_to_string(const GList *list,
                    GError     **error)
{
  GString *buffer;

  g_return_val_if_fail(error == NULL || *error == NULL, NULL);

  if (list == NULL)
    return g_strndup(COIL_STATIC_STRLEN("[]"));

  buffer = g_string_sized_new(128);
  coil_list_build_string(list, buffer, error);

  return g_string_free(buffer, FALSE);
}

