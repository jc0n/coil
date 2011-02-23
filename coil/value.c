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

static void
coil_none_class_init(CoilNoneClass *klass)
{
}

static void
coil_none_init(CoilNone *obj)
{
}

/* Prototypes */

static gint
_compare_value_list(const GValue *v1,
                    const GValue *v2,
                    GError      **error) G_GNUC_CONST;

/* Functions */

GValue *
value_alloc(void)
{
  return g_slice_new0(GValue);
}

inline GValue *
copy_value(const GValue *value)
{
  GValue *copy;

  g_return_val_if_fail(G_IS_VALUE(value), NULL);

  copy = value_alloc();
  g_value_init(copy, G_VALUE_TYPE(value));
  g_value_copy(value, copy);

  return copy;
}

GList *
copy_value_list(const GList *value_list)
{
  if (value_list == NULL)
    return NULL;

  const GList *list;
  GList       *list_copy = NULL;

  for (list = g_list_last((GList *)value_list);
       list; list = g_list_previous(list))
  {
    const GValue *value = (GValue *)list->data;
    GValue       *value_copy = copy_value(value);
    list_copy = g_list_prepend(list_copy, value_copy);
  }

  return list_copy;
}

inline void
free_value(gpointer value)
{
  if (value == NULL)
    return;

  g_return_if_fail(G_IS_VALUE(value));

  g_value_unset((GValue *)value);
  g_slice_free(GValue, value);
}


void
free_value_list(GList *list)
{
  while (list)
  {
    free_value(list->data);
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

static void
string_value_helper(GString *const buffer,
                    const gchar   *string,
                    guint          length)
{
  g_return_if_fail(buffer);
  g_return_if_fail(string);
  g_return_if_fail(length);

  if (length > COIL_MULTILINE_LEN
    || memchr(string, '\n', length) != NULL)
  {
    g_string_append_len(buffer,
                        COIL_STATIC_STRLEN(COIL_MULTILINE_QUOTE_S));
    g_string_append_len(buffer, string, length);
    g_string_append_len(buffer,
                        COIL_STATIC_STRLEN(COIL_MULTILINE_QUOTE_S));
  }
  else
  {
    g_string_append_c(buffer, '\'');
    g_string_append_len(buffer, string, length);
    g_string_append_c(buffer, '\'');
  }
}

COIL_API(void)
coil_value_build_string(const GValue *value,
                        GString      *const buffer,
                        GError      **error)
{
  g_return_if_fail(buffer != NULL);
  g_return_if_fail(error == NULL || *error == NULL);

  if (value == NULL)
  {
    g_string_append_len(buffer, COIL_STATIC_STRLEN("(null)"));
    return;
  }

  const GType type = G_VALUE_TYPE(value);

  if (g_value_type_transformable(type, G_TYPE_STRING))
  {
    switch (type)
    {
      case G_TYPE_BOOLEAN:
      {
        if (g_value_get_boolean(value))
          g_string_append_len(buffer, COIL_STATIC_STRLEN("True"));
        else
          g_string_append_len(buffer, COIL_STATIC_STRLEN("False"));
        break;
      }

      case G_TYPE_STRING:
      {
        const gchar *string;
        guint        length;

        string = g_value_get_string(value);
        length = strlen(string);

        string_value_helper(buffer, string, length);
        break;
      }

      default:
      {
        GValue cast_value = {0, };
        g_value_init(&cast_value, G_TYPE_STRING);
        g_value_transform(value, &cast_value);

        g_string_append(buffer, g_value_get_string(&cast_value));
        g_value_unset(&cast_value);
      }
    }
  }
  else
    switch (G_TYPE_FUNDAMENTAL(type))
    {
      case G_TYPE_OBJECT:
      {
        if (type == COIL_TYPE_NONE)
        {
          g_string_append_len(buffer, COIL_STATIC_STRLEN("None"));
          return;
        }
        else
        {
          GObject *obj = g_value_get_object(value);

          if (G_LIKELY(COIL_IS_EXPANDABLE(obj)))
          {
            coil_expandable_build_string(COIL_EXPANDABLE(obj), buffer, error);
            return;
          }

          g_assert_not_reached();
        }
        break;
      }

      case G_TYPE_BOXED:
      {
        if (type == G_TYPE_GSTRING)
        {
          const GString *gstring = (GString *)g_value_get_boxed(value);
          string_value_helper(buffer, gstring->str, gstring->len);
        }
        else if (type == COIL_TYPE_LIST)
        {
          const GList *list = (GList *)g_value_get_boxed(value);
          coil_list_build_string(list, buffer, error);
        }
        else if (type == COIL_TYPE_PATH)
        {
          const CoilPath *path = (CoilPath *)g_value_get_boxed(value);
          g_string_append_len(buffer, path->path, path->path_len);
        }
        else
          g_assert_not_reached();
        break;
      }
    }
}

COIL_API(gchar *)
coil_value_to_string(const GValue *value,
                     GError      **error)
{
  g_return_val_if_fail(error == NULL || *error == NULL, NULL);

  GString *buffer = g_string_sized_new(128);
  coil_value_build_string(value, buffer, error);

  return g_string_free(buffer, FALSE);
}

static gint
_compare_value_list(const GValue *v1,
                    const GValue *v2,
                    GError      **error)
{
  const GList *lp1, *lp2;
  gint         last;

  g_return_val_if_fail(G_IS_VALUE(v1), -1);
  g_return_val_if_fail(G_IS_VALUE(v2), -1);

  if (v1 == v2)
    return 0;

  lp1 = (GList *)g_value_get_boxed(v1);
  lp2 = (GList *)g_value_get_boxed(v2);

  while (lp1 && lp2)
  {
    const GValue *v1 = (GValue *)lp1->data;
    const GValue *v2 = (GValue *)lp2->data;

    if ((G_VALUE_HOLDS(v1, COIL_TYPE_EXPANDABLE)
        && !coil_expand_value(v1, &v1, TRUE, error))
      || (G_VALUE_HOLDS(v2, COIL_TYPE_EXPANDABLE)
        && !coil_expand_value(v2, &v2, TRUE, error))
      || (last = coil_value_compare(v1, v2, error)))
      break;

    lp1 = g_list_next(lp1);
    lp2 = g_list_next(lp2);
  }

  if (lp1 || lp2)
    return last;

  return 0;
}

COIL_API(gint)
coil_value_compare(const GValue *v1,
                   const GValue *v2,
                   GError      **error)
{
  if (v1 == v2)
    return 0;

  if (v1 == NULL || v2 == NULL)
    return (v1) ? 1 : -1;

  GType t1, t2;

  t1 = G_VALUE_TYPE(v1);
  t2 = G_VALUE_TYPE(v2);

start:
  if (t1 == t2)
    switch (G_TYPE_FUNDAMENTAL(t1))
    {
      case G_TYPE_NONE:
        return 0;

      case G_TYPE_CHAR:
      {
        register gchar c1, c2;
        c1 = g_value_get_char(v1);
        c2 = g_value_get_char(v2);
        return (c1 > c2) ? 1 : (c1 == c2) ? 0 : -1;
      }

      case G_TYPE_UCHAR:
      {
        register guchar c1, c2;
        c1 = g_value_get_uchar(v1);
        c2 = g_value_get_uchar(v2);
        return (c1 > c2) ? 1 : (c1 == c2) ? 0 : -1;
      }

      case G_TYPE_BOOLEAN:
      {
        register gboolean b1, b2;
        b1 = g_value_get_boolean(v1);
        b2 = g_value_get_boolean(v2);
        return (b1 > b2) ? 1 : (b1 == b2) ? 0 : -1;
      }

      case G_TYPE_INT:
      {
        register gint i1, i2;
        i1 = g_value_get_int(v1);
        i2 = g_value_get_int(v2);
        return (i1 > i2) ? 1 : (i1 == i2) ? 0 : -1;
      }

      case G_TYPE_UINT:
      {
        register guint u1, u2;
        u1 = g_value_get_uint(v1);
        u2 = g_value_get_uint(v2);
        return (u1 > u2) ? 1 : (u1 == u2) ? 0 : -1;
      }

      case G_TYPE_LONG:
      {
        register glong l1, l2;
        l1 = g_value_get_long(v1);
        l2 = g_value_get_long(v2);
        return (l1 > l2) ? 1 : (l1 == l2) ? 0 : -1;
      }

      case G_TYPE_ULONG:
      {
        register gulong ul1, ul2;
        ul1 = g_value_get_ulong(v1);
        ul2 = g_value_get_ulong(v2);
        return (ul1 > ul2) ? 1 : (ul1 == ul2) ? 0 : -1;
      }

      case G_TYPE_INT64:
      {
        register gint64 i1, i2;
        i1 = g_value_get_int64(v1);
        i2 = g_value_get_int64(v2);
       return (i1 > i2) ? 1 : (i1 == i2) ? 0 : -1;
      }

      case G_TYPE_UINT64:
      {
        register guint64 u1, u2;
        u1 = g_value_get_uint64(v1);
        u2 = g_value_get_uint64(v2);
        return (u1 > u2) ? 1 : (u1 == u2) ? 0 : -1;
      }

      case G_TYPE_FLOAT:
      {
        register gfloat f1, f2;
        f1 = g_value_get_float(v1);
        f2 = g_value_get_float(v2);
        return (f1 > f2) ? 1 : (f1 == f2) ? 0 : -1;
      }

      case G_TYPE_DOUBLE:
      {
        register gdouble d1, d2;
        d1 = g_value_get_double(v1);
        d2 = g_value_get_double(v2);
        return (d1 > d2) ? 1 : (d1 == d2) ? 0 : -1;
      }

      case G_TYPE_STRING:
      {
        register const gchar *s1, *s2;
        s1 = g_value_get_string(v1);
        s2 = g_value_get_string(v2);
        return strcmp(s1, s2);
      }

      case G_TYPE_POINTER:
      {
        register gpointer *p1, *p2;
        p1 = g_value_get_pointer(v1);
        p2 = g_value_get_pointer(v2);
        return (p1 > p2) ? 1 : (p1 == p2) ? 0 : -1;
      }

      case G_TYPE_OBJECT:
      {
        if (t1 == COIL_TYPE_NONE)
            return 0;
        else if (g_type_is_a(t1, COIL_TYPE_EXPANDABLE))
        {
          GObject *o1, *o2;
          o1 = g_value_get_object(v1);
          o2 = g_value_get_object(v2);
          return !coil_expandable_equals(o1, o2, error);
        }
/*        else if (t1 == COIL_TYPE_STRUCT)
        {
          CoilStruct    *s1, *s2;
          s1 = COIL_STRUCT(g_value_get_object(v1));
          s2 = COIL_STRUCT(g_value_get_object(v2));

          return (coil_struct_equals(s1, s2, error) == TRUE) ? 0 : 1;
        }
*/
        goto bad_type;
      }

      case G_TYPE_BOXED:
      {
        if (t1 == G_TYPE_GSTRING)
        {
          const GString *s1, *s2;
          s1 = (GString *)g_value_get_boxed(v1);
          s2 = (GString *)g_value_get_boxed(v2);

          if (s1->len == s2->len)
            return memcmp(s1->str, s2->str, s1->len);

          return s1->len > s2->len ? 1 : -1;
        }
        else if (t1 == COIL_TYPE_LIST)
          return _compare_value_list(v1, v2, error);
      }

      default:
        goto bad_type;
    }
  else
  {
    if (g_type_is_a(t1, COIL_TYPE_EXPANDABLE))
    {
      if (!coil_expand_value(v1, &v1, TRUE, error))
        return -1;

      goto start;
    }
    else if (g_type_is_a(t2, COIL_TYPE_EXPANDABLE))
    {
      if (!coil_expand_value(v2, &v2, TRUE, error))
        return -1;

      goto start;
    }
  }

 return (t1 > t2) ? 1 : -1;

bad_type:
#ifdef COIL_DEBUG
        g_debug("value GType = %d", (gint)t1);
#endif
        COIL_NOT_IMPLEMENTED(-1);
}

