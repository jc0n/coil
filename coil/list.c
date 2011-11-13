/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */

#include "common.h"

#include "list.h"
#include "value.h"

COIL_API(void)
coil_list_build_string(CoilList *list,
                       GString *const buffer,
                       CoilStringFormat *_format,
                       GError **error)
{
    g_return_if_fail(list);
    g_return_if_fail(buffer);
    g_return_if_fail(_format);
    g_return_if_fail(error == NULL || *error == NULL);

    guint i, n, delim_len, opts;
    gchar delim[128];
    CoilStringFormat format = *_format;
    GError *internal_error = NULL;

    n = list->n_values;
    if (n == 0) {
        g_string_append_len(buffer, "[]", 2);
        return;
    }

    opts = format.options;
    delim[0] = (opts & COMMAS_IN_LIST) ? ',' : ' ';
    delim_len = 1;

    if (opts & BLANK_LINE_AFTER_ITEM) {
        gsize width;

        delim[1] = '\n';
        delim_len = 2;
        format.indent_level += format.block_indent;
        width = MIN(sizeof(delim) - delim_len, format.indent_level);
        memset(delim + delim_len, ' ', width);
        delim_len += width;
    }

    g_string_append_c(buffer, '[');

    if (opts & BLANK_LINE_AFTER_ITEM)
        g_string_append_len(buffer, delim, delim_len);

    i = 0;
    do {
        GValue *value = g_value_array_get_nth(list, i);
        coil_value_build_string(value, buffer, &format, &internal_error);
        if (internal_error) {
            g_propagate_error(error, internal_error);
            return;
        }
        g_string_append_len(buffer, delim, delim_len);
    } while (++i < n);

    if (!(opts & LIST_ON_BLANK_LINE))
        g_string_truncate(buffer, buffer->len - delim_len);

    g_string_append_c(buffer, ']');
}

COIL_API(gchar *)
coil_list_to_string(CoilList *list,
                    CoilStringFormat *format,
                    GError **error)
{
  GString *buffer;

  if (list->n_values == 0)
    return g_strndup("[]", 2);

  buffer = g_string_sized_new(128);
  coil_list_build_string(list, buffer, format, error);

  return g_string_free(buffer, FALSE);
}

