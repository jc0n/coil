/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */
#include "string.h"

#include "common.h"

#include "list.h"
#include "struct.h"
#include "value.h"

G_DEFINE_TYPE(CoilNone, coil_none, G_TYPE_OBJECT);

CoilNone *coil_none_object = NULL;

static gint
_compare_value_list(const GValue *v1,
                    const GValue *v2,
                    GError      **error);

static void
noneval_to_strval(const GValue *noneval,
                        GValue *strval)
{
  g_return_if_fail(G_IS_VALUE(noneval));
  g_return_if_fail(G_IS_VALUE(strval));

  g_value_set_static_string(strval, "None");
}

static void
coil_none_class_init(CoilNoneClass *klass)
{
  g_value_register_transform_func(COIL_TYPE_NONE, G_TYPE_STRING,
                                  noneval_to_strval);
}

static void
coil_none_init(CoilNone *obj)
{
}

GValue *
coil_value_alloc(void)
{
  return g_slice_new0(GValue);
}

inline GValue *
coil_value_copy(const GValue *value)
{
  GValue *copy;

  g_return_val_if_fail(G_IS_VALUE(value), NULL);

  copy = coil_value_alloc();
  g_value_init(copy, G_VALUE_TYPE(value));
  g_value_copy(value, copy);

  return copy;
}

GList *
coil_value_list_copy(const GList *value_list)
{
  const GList  *list;
  GList        *list_copy = NULL;
  const GValue *value;
  GValue       *value_copy;

  for (list = g_list_last((GList *)value_list);
       list; list = g_list_previous(list))
  {
    value = (GValue *)list->data;
    value_copy = coil_value_copy(value);
    list_copy = g_list_prepend(list_copy, value_copy);
  }

  return list_copy;
}

inline void
coil_value_free(gpointer value)
{
  if (value == NULL)
    return;

  g_return_if_fail(G_IS_VALUE(value));

  g_value_unset((GValue *)value);
  g_slice_free(GValue, value);
}


void
coil_value_list_free(GList *list)
{
  while (list)
  {
    coil_value_free(list->data);
    list = g_list_delete_link(list, list);
  }
}

inline void
free_string_list(GList *list)
{
  while (list)
  {
    g_free(list);
    list = g_list_delete_link(list, list);
  }
}

static GString *
buffer_string_append_escaped(GString     *buffer,
                             const gchar *string,
                             gssize       length)
{
  g_return_val_if_fail(buffer, NULL);
  g_return_val_if_fail(string, NULL);

  const gchar *p;

  for (p = string; length-- > 0; string++)
    switch (*string)
    {
      case '\0':
        g_warning("unexpected nul '\\0' byte");
        return buffer;

      case '\'':
      case '$':
        g_string_append_len(buffer, p, string - p);
        g_string_append_c(buffer, '\\');
        p = string;
    }

  g_string_append_len(buffer, p, string - p);

  return buffer;
}

static void
buffer_string_append(GString *const    buffer,
                     const gchar      *string,
                     guint             length,
                     CoilStringFormat *format)
{
  g_return_if_fail(buffer);
  g_return_if_fail(string);
  g_return_if_fail(format);

  GString *(*append_func)(GString *, const gchar *, gssize);

  if (format->options & ESCAPE_QUOTES)
    append_func = buffer_string_append_escaped;
  else
    append_func = g_string_append_len;

  if (format->options & DONT_QUOTE_STRINGS)
  {
    append_func(buffer, string, length);
    return;
  }

  if (length > format->multiline_len
    || memchr(string, '\n', length) != NULL)
  {
    g_string_append_len(buffer, COIL_STATIC_STRLEN(COIL_MULTILINE_QUOTE));
    append_func(buffer, string, length);
    g_string_append_len(buffer, COIL_STATIC_STRLEN(COIL_MULTILINE_QUOTE));
  }
  else
  {
    g_string_append_c(buffer, '\'');
    append_func(buffer, string, length);
    g_string_append_c(buffer, '\'');
  }
}

COIL_API(void)
coil_value_build_string(const GValue     *value,
                        GString          *const buffer,
                        CoilStringFormat *format,
                        GError          **error)
{
  g_return_if_fail(value);
  g_return_if_fail(buffer);
  g_return_if_fail(format);
  g_return_if_fail(error == NULL || *error == NULL);

  if (format->options & FORCE_EXPAND
    && G_VALUE_HOLDS(value, COIL_TYPE_EXPANDABLE)
    && !coil_expand_value(value, &value, TRUE, error))
      return;

  const GType type = G_VALUE_TYPE(value);

  if (G_TYPE_IS_FUNDAMENTAL(type))
  {
    if (type == G_TYPE_BOOLEAN)
    {
      if (g_value_get_boolean(value))
        g_string_append_len(buffer, COIL_STATIC_STRLEN("True"));
      else
        g_string_append_len(buffer, COIL_STATIC_STRLEN("False"));

      return;
    }

    if (type == G_TYPE_STRING)
    {
      const gchar *string;
      guint        length;

      string = g_value_get_string(value);
      length = strlen(string);

      buffer_string_append(buffer, string, length, format);
      return;
    }

    goto transform;
  }

  if (g_type_is_a(type, COIL_TYPE_EXPANDABLE))
  {
    CoilExpandable *object;

    object = COIL_EXPANDABLE(g_value_get_object(value));
    coil_expandable_build_string(object, buffer, format, error);
    return;
  }

  if (g_type_is_a(type, G_TYPE_BOXED))
  {
    if (type == G_TYPE_GSTRING)
    {
      const GString *gstring = (GString *)g_value_get_boxed(value);
      buffer_string_append(buffer, gstring->str, gstring->len, format);
      return;
    }

    if (type == COIL_TYPE_LIST)
    {
      const GList *list = (GList *)g_value_get_boxed(value);
      coil_list_build_string(list, buffer, format, error);
      return;
    }

    if (type == COIL_TYPE_PATH)
    {
      const CoilPath *path = (CoilPath *)g_value_get_boxed(value);
      g_string_append_len(buffer, path->path, path->path_len);
      return;
    }
  }

transform:
  if (g_value_type_transformable(type, G_TYPE_STRING))
  {
    GValue tmp = {0, };

    g_value_init(&tmp, G_TYPE_STRING);
    g_value_transform(value, &tmp);

    g_string_append(buffer, g_value_get_string(&tmp));
    g_value_unset(&tmp);
    return;
  }

  if (g_value_type_compatible(type, G_TYPE_STRING))
  {
    GValue tmp = {0, };

    g_value_init(&tmp, G_TYPE_STRING);
    g_value_copy(value, &tmp);

    g_string_append(buffer, g_value_get_string(&tmp));
    g_value_unset(&tmp);
    return;
  }

  g_warning("Unable to build string for value type %s",
            G_VALUE_TYPE_NAME(value));
}

COIL_API(gchar *)
coil_value_to_string(const GValue     *value,
                     CoilStringFormat *format,
                     GError          **error)
{
  g_return_val_if_fail(error == NULL || *error == NULL, NULL);

  GString *buffer = g_string_sized_new(128);
  coil_value_build_string(value, buffer, format, error);

  return g_string_free(buffer, FALSE);
}

static gint
_compare_value_list(const GValue *v1,
                    const GValue *v2,
                    GError      **error)
{
  g_return_val_if_fail(G_IS_VALUE(v1), -1);
  g_return_val_if_fail(G_IS_VALUE(v2), -1);
  g_return_val_if_fail(G_VALUE_HOLDS(v1, COIL_TYPE_LIST), -1);
  g_return_val_if_fail(G_VALUE_HOLDS(v2, COIL_TYPE_LIST), -1);

  const GList *lp1, *lp2;

  if (v1 == v2)
    return 0;

  lp1 = (GList *)g_value_get_boxed(v1);
  lp2 = (GList *)g_value_get_boxed(v2);

  while (lp1 && lp2)
  {
    v1 = (GValue *)lp1->data;
    v2 = (GValue *)lp2->data;

    if (coil_value_compare(v1, v2, error))
      break;

    lp1 = g_list_next(lp1);
    lp2 = g_list_next(lp2);
  }

  if (lp1)
    return 1;

  if (lp2)
    return -1;

  return 0;
}

static void
__bad_comparetype(GType t1,
                  GType t2)
{
  g_error("Invalid or unimplemented comparison types, t1 = %s, t2 = %s",
          g_type_name(t1),
          g_type_name(t2));
}

/* TODO(jcon): see about having this added to glib */
static gint
g_string_compare(const GString *a,
                 const GString *b)
{
  g_return_val_if_fail(a, -1);
  g_return_val_if_fail(b, -1);

  if (a->len == b->len)
    return memcmp(a->str, b->str, b->len);

  return (a->len > b->len) ? 1 : -1;
}

static gint
value_compare_as_fundamental(const GValue *v1,
                             const GValue *v2,
                             GError      **error)
{
  g_return_val_if_fail(G_IS_VALUE(v1), -1);
  g_return_val_if_fail(G_IS_VALUE(v2), -1);
  g_return_val_if_fail(error == NULL || *error == NULL, -1);

  GType t1, t2;

  t1 = G_VALUE_TYPE(v1);
  t2 = G_VALUE_TYPE(v2);

  g_return_val_if_fail(t1 == t2, -1);

  switch (G_TYPE_FUNDAMENTAL(t1))
  {
    case G_TYPE_NONE:
      return 0;

    case G_TYPE_CHAR:
    {
      gchar c1, c2;
      c1 = g_value_get_char(v1);
      c2 = g_value_get_char(v2);
      return (c1 > c2) ? 1 : (c1 == c2) ? 0 : -1;
    }

    case G_TYPE_UCHAR:
    {
      guchar c1, c2;
      c1 = g_value_get_uchar(v1);
      c2 = g_value_get_uchar(v2);
      return (c1 > c2) ? 1 : (c1 == c2) ? 0 : -1;
    }

    case G_TYPE_BOOLEAN:
    {
      gboolean b1, b2;
      b1 = g_value_get_boolean(v1);
      b2 = g_value_get_boolean(v2);
      return (b1 > b2) ? 1 : (b1 == b2) ? 0 : -1;
    }

    case G_TYPE_INT:
    {
      gint i1, i2;
      i1 = g_value_get_int(v1);
      i2 = g_value_get_int(v2);
      return (i1 > i2) ? 1 : (i1 == i2) ? 0 : -1;
    }

    case G_TYPE_UINT:
    {
      guint u1, u2;
      u1 = g_value_get_uint(v1);
      u2 = g_value_get_uint(v2);
      return (u1 > u2) ? 1 : (u1 == u2) ? 0 : -1;
    }

    case G_TYPE_LONG:
    {
      glong l1, l2;
      l1 = g_value_get_long(v1);
      l2 = g_value_get_long(v2);
      return (l1 > l2) ? 1 : (l1 == l2) ? 0 : -1;
    }

    case G_TYPE_ULONG:
    {
      gulong ul1, ul2;
      ul1 = g_value_get_ulong(v1);
      ul2 = g_value_get_ulong(v2);
      return (ul1 > ul2) ? 1 : (ul1 == ul2) ? 0 : -1;
    }

    case G_TYPE_INT64:
    {
      gint64 i1, i2;
      i1 = g_value_get_int64(v1);
      i2 = g_value_get_int64(v2);
     return (i1 > i2) ? 1 : (i1 == i2) ? 0 : -1;
    }

    case G_TYPE_UINT64:
    {
      guint64 u1, u2;
      u1 = g_value_get_uint64(v1);
      u2 = g_value_get_uint64(v2);
      return (u1 > u2) ? 1 : (u1 == u2) ? 0 : -1;
    }

    case G_TYPE_FLOAT:
    {
      gfloat f1, f2;
      f1 = g_value_get_float(v1);
      f2 = g_value_get_float(v2);
      return (f1 > f2) ? 1 : (f1 == f2) ? 0 : -1;
    }

    case G_TYPE_DOUBLE:
    {
      gdouble d1, d2;
      d1 = g_value_get_double(v1);
      d2 = g_value_get_double(v2);
      return (d1 > d2) ? 1 : (d1 == d2) ? 0 : -1;
    }

    case G_TYPE_STRING:
    {
      const gchar *s1, *s2;
      s1 = g_value_get_string(v1);
      s2 = g_value_get_string(v2);
      return strcmp(s1, s2);
    }

    case G_TYPE_POINTER:
    {
      gpointer *p1, *p2;
      p1 = g_value_get_pointer(v1);
      p2 = g_value_get_pointer(v2);
      return (p1 > p2) ? 1 : (p1 == p2) ? 0 : -1;
    }

    case G_TYPE_OBJECT:
    {
      if (t1 == COIL_TYPE_NONE)
          return 0;

      if (g_type_is_a(t1, COIL_TYPE_EXPANDABLE))
      {
        GObject *o1, *o2;

        o1 = g_value_get_object(v1);
        o2 = g_value_get_object(v2);

        if (o1 == o2)
          return 0;

        return !coil_expandable_equals(o1, o2, error);
      }

      break;
    }

    case G_TYPE_BOXED:
    {
      if (t1 == G_TYPE_GSTRING)
      {
        const GString *s1, *s2;
        s1 = (GString *)g_value_get_boxed(v1);
        s2 = (GString *)g_value_get_boxed(v2);
        return g_string_compare(s1, s2);
      }

      if (t1 == COIL_TYPE_LIST)
        return _compare_value_list(v1, v2, error);

      break;
    }
  }

  __bad_comparetype(t1, t2);
  return -1;
}

static gint
value_compare_as_string(const GValue  *v1,
                        const GValue  *v2,
                        GError       **error)
{
  g_return_val_if_fail(G_IS_VALUE(v1), -1);
  g_return_val_if_fail(G_IS_VALUE(v2), -1);
  g_return_val_if_fail(error == NULL || *error == NULL, -1);

  GType        t1, t2;
  const gchar *s1, *s2;
  gint         result;

  t1 = G_VALUE_TYPE(v1);
  t2 = G_VALUE_TYPE(v2);

  if (t1 == G_TYPE_STRING && t2 == G_TYPE_STRING)
  {
    s1 = g_value_get_string(v1);
    s2 = g_value_get_string(v2);
    result = strcmp(s1, s2);
  }
  else if (t1 == G_TYPE_STRING && t2 == G_TYPE_GSTRING)
  {
    s1 = g_value_get_string(v1);
    s2 = ((GString *)g_value_get_boxed(v2))->str;
    result = strcmp(s1, s2);
  }
  else if (t2 == G_TYPE_STRING && t1 == G_TYPE_GSTRING)
  {
    s1 = ((GString *)g_value_get_boxed(v1))->str;
    s2 = g_value_get_string(v2);
    result = strcmp(s1, s2);
  }
  else if (g_value_type_transformable(t1, G_TYPE_STRING)
    && g_value_type_transformable(t2, G_TYPE_STRING))
  {
    gchar *s1, *s2;

    s1 = g_strdup_value_contents(v1);
    s2 = g_strdup_value_contents(v2);

    result = strcmp(s1, s2);

    g_free(s1);
    g_free(s2);
  }
  else
    __bad_comparetype(t1, t2);

  return result;
}

COIL_API(gint)
coil_value_compare(const GValue *v1,
                   const GValue *v2,
                   GError      **error)
{
  GType    t1, t2;
  gboolean expanded = FALSE;

  if (v1 == v2)
    return 0;

  if (v1 == NULL || v2 == NULL)
    return (v1) ? 1 : -1;

start:
  t1 = G_VALUE_TYPE(v1);
  t2 = G_VALUE_TYPE(v2);

  if (t1 == t2)
    return value_compare_as_fundamental(v1, v2, error);

  if (!expanded)
  {
    if (g_type_is_a(t1, COIL_TYPE_EXPANDABLE) &&
      !coil_expand_value(v1, &v1, TRUE, error))
      return -1;

    if (g_type_is_a(t2, COIL_TYPE_EXPANDABLE) &&
      !coil_expand_value(v2, &v2, TRUE, error))
      return -1;

    expanded = TRUE;
    goto start;
  }

  return value_compare_as_string(v1, v2, error);
}

