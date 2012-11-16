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
#include "common.h"

#include "list.h"
#include "value.h"

G_DEFINE_TYPE(CoilList, coil_list, COIL_TYPE_OBJECT);

#define COIL_LIST_GET_PRIVATE(o) \
    (G_TYPE_INSTANCE_GET_PRIVATE((o), COIL_TYPE_LIST, CoilListPrivate))

struct _CoilListPrivate
{
    GArray *arr;
};

static void
clear_func(gpointer value_pointer)
{
    CoilValue *value = *((GValue **)value_pointer);
    coil_value_free(value);
}

COIL_API(CoilObject *)
coil_list_new_sized(guint n)
{
    GArray *arr;
    CoilObject *self;

    self = COIL_OBJECT(g_object_newv(COIL_TYPE_LIST, 0, NULL));

    arr = g_array_sized_new(FALSE, FALSE, sizeof(CoilValue *), n);
    g_array_set_clear_func(arr, clear_func);
    COIL_LIST(self)->priv->arr = arr;

    return self;
}

COIL_API(CoilObject *)
coil_list_new(void)
{
    return coil_list_new_sized(0);
}

#define G_ARRAY_FUNC(list, func, args...) \
    g_assert(list != NULL); \
    g_assert(COIL_IS_LIST(list)); \
    G_PASTE(g_array_, func)(COIL_LIST(list)->priv->arr, ##args)

COIL_API(CoilObject *) /* steals reference */
coil_list_append(CoilObject *list, CoilValue *value)
{
    G_ARRAY_FUNC(list, append_val, value);
    return list;
}

COIL_API(CoilObject *) /* steals reference */
coil_list_prepend(CoilObject *list, CoilValue *value)
{
    G_ARRAY_FUNC(list, prepend_val, value);
    return list;
}

COIL_API(CoilObject *)
coil_list_remove_range(CoilObject *list, guint i, guint n)
{
    G_ARRAY_FUNC(list, remove_range, i, n);
    return list;
}

COIL_API(CoilValue *) /* borrowed reference */
coil_list_get_index(CoilObject *list, guint i)
{
    g_return_val_if_fail(list != NULL, NULL);
    g_return_val_if_fail(COIL_IS_LIST(list), NULL);
    g_return_val_if_fail(i < coil_list_length(list), NULL);

    return g_array_index(COIL_LIST(list)->priv->arr, CoilValue *, i);
}

COIL_API(CoilValue *) /* new reference */
coil_list_dup_index(CoilObject *list, guint i)
{
    GValue *v;

    v = coil_list_get_index(list, i);
    return coil_value_copy(v);
}

COIL_API(guint)
coil_list_length(CoilObject *list)
{
    g_return_val_if_fail(list != NULL, 0);
    g_return_val_if_fail(COIL_IS_LIST(list), 0);

    return COIL_LIST(list)->priv->arr->len;
}

static void
list_build_string(CoilObject *list, GString *buffer, CoilStringFormat *_format)
{
    g_return_if_fail(list != NULL);
    g_return_if_fail(buffer != NULL);
    g_return_if_fail(COIL_IS_LIST(list));

    guint i, n, delim_len, opts;
    gchar delim[128];
    CoilStringFormat format;

    n = coil_list_length(list);
    if (n == 0) {
        g_string_append_len(buffer, "[]", 2);
        return;
    }
    format = (_format) ? *_format : default_string_format;
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
        GValue *value = coil_list_get_index(list, i);
        coil_value_build_string(value, buffer, &format);
        if (coil_error_occurred()) {
            return;
        }
        g_string_append_len(buffer, delim, delim_len);
    } while (++i < n);

    if (!(opts & LIST_ON_BLANK_LINE))
        g_string_truncate(buffer, buffer->len - delim_len);

    g_string_append_c(buffer, ']');
}

static gboolean
list_equals(CoilObject *listA, CoilObject *listB)
{
    g_return_val_if_fail(listA != NULL, FALSE);
    g_return_val_if_fail(listB != NULL, FALSE);
    g_return_val_if_fail(COIL_IS_LIST(listA), FALSE);
    g_return_val_if_fail(COIL_IS_LIST(listB), FALSE);

    guint i, len;

    len = coil_list_length(listA);
    if (len != coil_list_length(listB)) {
        return FALSE;
    }
    for (i = 0; i < len; i++) {
        GValue *valueA, *valueB;
        valueA = coil_list_get_index(listA, i);
        valueB = coil_list_get_index(listB, i);
        if (!coil_value_equals(valueA, valueB)) {
            return FALSE;
        }
    }
    return TRUE;
}

static void
list_set_container(CoilObject *list, CoilObject *container)
{
    g_return_if_fail(list != NULL);
    g_return_if_fail(COIL_IS_LIST(list));

    guint i, len;

    len = coil_list_length(list);
    for (i = 0; i < len; i++) {
        CoilValue *value;
        CoilObject *object;

        value = coil_list_get_index(list, i);
        if (!G_VALUE_HOLDS(value, COIL_TYPE_OBJECT)) {
            continue;
        }
        object = coil_value_get_object(value);
        coil_object_set_container(object, container);
    }
}

static CoilObject *
list_copy(CoilObject *list, const gchar *first_property_name, va_list properties)
{
    g_return_val_if_fail(list != NULL, NULL);
    g_return_val_if_fail(COIL_IS_LIST(list), NULL);

    CoilObject *copy;
    guint i, len;

    len = coil_list_length(list);
    copy = coil_list_new_sized(len);
    for (i = 0; i < len; i++) {
        CoilValue *value = coil_list_dup_index(list, i);
        coil_list_append(copy, value);
    }
    return copy;
}

static void
list_dispose(GObject *object)
{
    CLEAR(COIL_LIST(object)->priv->arr, g_array_free, TRUE);
    G_OBJECT_CLASS(coil_list_parent_class)->dispose(object);
}

static void
coil_list_init(CoilList *self)
{
    self->priv = COIL_LIST_GET_PRIVATE(self);
}

static void
coil_list_class_init(CoilListClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    CoilObjectClass *object_class = COIL_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(CoilListPrivate));

    gobject_class->dispose = list_dispose;

    object_class->set_container = list_set_container;
    object_class->build_string = list_build_string;
    object_class->equals = list_equals;
    object_class->copy = list_copy;
}

