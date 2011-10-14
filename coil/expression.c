/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */

#include "common.h"

#include <string.h>

#include "struct.h"
#include "expandable.h"
#include "expression.h"

G_DEFINE_TYPE(CoilExpr, coil_expr, COIL_TYPE_EXPANDABLE);

#define COIL_EXPR_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE((obj), COIL_TYPE_EXPR, CoilExprPrivate))

struct _CoilExprPrivate
{
  GString   *expr;
  GValue    *expanded_value;
  gboolean   is_expanded : 1;
};

static gboolean
expr_is_expanded(gconstpointer object)
{
  g_return_val_if_fail(COIL_IS_EXPR(object), FALSE);

  return COIL_EXPR(object)->priv->is_expanded;
}

static void
append_path_substitution(CoilExpr         *self,
                         GString          *buffer,
                         CoilStringFormat *format,
                         const gchar      *path,
                         guint             len,
                         GError          **error)
{
  g_return_if_fail(COIL_IS_EXPR(self));
  g_return_if_fail(path != NULL);
  g_return_if_fail(len > 0);

  CoilStruct   *container = COIL_EXPANDABLE(self)->container;
  const GValue *value;
  GError       *internal_error = NULL;

  value = coil_struct_lookup(container, path, len, TRUE, &internal_error);

  if (G_UNLIKELY(internal_error))
  {
    g_propagate_error(error, internal_error);
    return;
  }

  if (value == NULL)
    return;

  coil_value_build_string(value, buffer, format, NULL);
}


static gboolean
expr_expand(gconstpointer  object,
            const GValue **return_value,
            GError       **error)
{
  g_return_val_if_fail(COIL_IS_EXPR(object), FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  CoilExpr        *const self = COIL_EXPR(object);
  CoilExprPrivate *const priv = self->priv;
  CoilStringFormat format = default_string_format;
  GString         *expr = priv->expr, *buffer;
  const gchar     *s, *p;
  GError          *internal_error = NULL;

  if (priv->is_expanded)
    goto done;

  buffer = g_string_sized_new(128);

  format.indent_level = 0;
  format.options &= ~ESCAPE_QUOTES;
  format.options |= DONT_QUOTE_STRINGS;

  for (s = expr->str; *s; s++)
  {
    if (*s == '\\')
    {
      g_string_append_c(buffer, *++s);
      continue;
    }

    /**
     * TODO(jcon): Add more advanced possibly bash style
     * replacements here as well as list indexing
     */
    if (*s == '$' && s[1] == '{')
    {
      s += 2;
      /* XXX: safe b.c lexer has already found '}' */
      p = rawmemchr(s + 1, '}');

      append_path_substitution(self, buffer, &format, s, p - s, &internal_error);

      if (G_UNLIKELY(internal_error))
      {
        g_propagate_error(error, internal_error);
        g_string_free(buffer, TRUE);
        return FALSE;
      }

      s = p;
      continue;
    }

    g_string_append_c(buffer, *s);
  }

  coil_value_init(priv->expanded_value, G_TYPE_STRING,
            take_string, g_string_free(buffer, FALSE));

  priv->is_expanded = TRUE;

done:
  if (return_value)
    *return_value = priv->expanded_value;

  return TRUE;
}

static gboolean
expr_equals(gconstpointer  object,
            gconstpointer  other_object,
            GError       **error)
{
  g_return_val_if_fail(COIL_IS_EXPR(object), FALSE);
  g_return_val_if_fail(COIL_IS_EXPANDABLE(other_object), FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  const GValue  *v1, *v2;
  const gchar   *s1;
  gchar         *s2;
  gboolean       result;

  if (!coil_expand((gpointer)object, &v1, TRUE, error))
    return FALSE;

  s1 = g_value_get_string(v1);

  if (!coil_expand((gpointer)other_object, &v2, TRUE, error))
    return FALSE;

  s2 = g_strdup_value_contents(v2);

  result = g_str_equal(s1, s2);
  g_free(s2);

  return result;
}

static void
expr_build_string(gconstpointer     object,
                  GString          *buffer,
                  CoilStringFormat *format,
                  GError          **error)
{
  g_return_if_fail(COIL_IS_EXPR(object));
  g_return_if_fail(buffer != NULL);
  g_return_if_fail(format != NULL);
  g_return_if_fail(error == NULL || *error == NULL);

  const GValue *return_value = NULL;

  if (!coil_expand((gpointer)object, &return_value, TRUE, error))
    return;

  coil_value_build_string(return_value, buffer, format, error);
}

COIL_API(gchar *)
coil_expr_to_string(CoilExpr         *self,
                    CoilStringFormat *format,
                    GError          **error)
{
  g_return_val_if_fail(COIL_IS_EXPR(self), NULL);
  g_return_val_if_fail(error == NULL || *error == NULL, NULL);

  GString *buffer = g_string_sized_new(128);
  GError  *internal_error = NULL;

  expr_build_string(self, buffer, format, &internal_error);

  if (G_UNLIKELY(internal_error))
  {
    g_propagate_error(error, internal_error);
    return NULL;
  }

  return g_string_free(buffer, FALSE);
}

#if COIL_PATH_TRANSLATION
static gboolean
expr_translate_path(GString    *expr,
                    CoilStruct *old_container,
                    CoilStruct *new_container,
                    GError    **error)
{
  g_return_val_if_fail(COIL_IS_STRUCT(old_container), FALSE);
  g_return_val_if_fail(COIL_IS_STRUCT(new_container), FALSE);

  guint           i;
  const gchar    *s, *e;
  CoilPath       *path, *new_path;
  const CoilPath *container_path;

  for (i = 0, s = expr->str;
       i < expr->len; i++, s++)
  {
    if (*s == '\\')
    {
      s++;
      i++;
      continue;
    }

    if (*s == '$' && s[1] == '{')
    {
      s += 2;
      i += 2;

      e = rawmemchr(s + 1, '}');

      path = coil_path_new_len(s, e - s, error);
      if (path == NULL)
        return FALSE;

      container_path = coil_struct_get_path(old_container);
      new_path = coil_path_relativize(path, container_path);

      g_string_erase(expr, i, path->path_len);
      g_string_insert_len(expr, i, new_path->path, new_path->path_len);

      coil_path_unref(path);
      coil_path_unref(new_path);
    }
  }

  return TRUE;
}
#endif

static CoilExpandable *
expr_copy(gconstpointer     _self,
          const gchar      *first_property_name,
          va_list           properties,
          GError          **error)
{
  g_return_val_if_fail(COIL_IS_EXPANDABLE(_self), NULL);
  g_return_val_if_fail(error == NULL || *error == NULL, NULL);

  CoilExpr        *self = COIL_EXPR(_self);
  CoilExpr        *copy;
  CoilExprPrivate *priv = self->priv;
  GString         *string;

  string = g_string_new_len(priv->expr->str, priv->expr->len);
  copy = coil_expr_new_valist(string, first_property_name, properties);

#if COIL_PATH_TRANSLATION
  CoilStruct     *new_container, *old_container;

  new_container = COIL_EXPANDABLE(copy)->container;
  old_container = COIL_EXPANDABLE(self)->container;

  if (!coil_struct_compare_root(old_container, new_container)
    && !expr_translate_path(string, old_container, new_container, error))
      return NULL;
#endif

  return COIL_EXPANDABLE(copy);
}

static void
exprval_to_strval(const GValue *exprval,
                        GValue *strval)
{
  g_return_if_fail(G_IS_VALUE(exprval));
  g_return_if_fail(G_IS_VALUE(strval));

  CoilExpr *expr;
  gchar    *string;
  GError   *internal_error = NULL;

  expr = COIL_EXPR(g_value_get_object(exprval));
  string = coil_expr_to_string(expr, &default_string_format, &internal_error);

  if (G_UNLIKELY(internal_error))
  {
    g_warning("%s: %s", G_STRLOC, internal_error->message);
    g_error_free(internal_error);
    return;
  }

  g_value_take_string(strval, string);
}

COIL_API(CoilExpr *)
coil_expr_new(GString     *string, /* steals */
              const gchar *first_property_name,
              ...)
{
  CoilExpr *result;
  va_list   properties;

  va_start(properties, first_property_name);
  result = coil_expr_new_valist(string, first_property_name, properties);
  va_end(properties);

  return result;
}

COIL_API(CoilExpr *)
coil_expr_new_valist(GString     *string,
                     const gchar *first_property_name,
                     va_list      properties)
{
  GObject         *object;
  CoilExpr        *self;
  CoilExprPrivate *priv;

  object = g_object_new_valist(COIL_TYPE_EXPR, first_property_name, properties);

  self = COIL_EXPR(object);
  priv = self->priv;
  priv->expr = string;

  return self;
}

static void
coil_expr_finalize(GObject *object)
{
  CoilExpr        *const self = COIL_EXPR(object);
  CoilExprPrivate *const priv = self->priv;

  if (priv->expr)
    g_string_free(priv->expr, TRUE);

  if (priv->expanded_value)
    coil_value_free(priv->expanded_value);

  G_OBJECT_CLASS(coil_expr_parent_class)->finalize(object);
}

static void
coil_expr_init(CoilExpr *self)
{
  self->priv = COIL_EXPR_GET_PRIVATE(self);
}

static void
coil_expr_class_init(CoilExprClass *klass)
{
  GObjectClass        *gobject_class;
  CoilExpandableClass *expandable_class;

  gobject_class = G_OBJECT_CLASS(klass);
  expandable_class = COIL_EXPANDABLE_CLASS(klass);

  g_type_class_add_private(klass, sizeof(CoilExprPrivate));

  gobject_class->finalize = coil_expr_finalize;

  expandable_class->is_expanded = expr_is_expanded;
  expandable_class->expand = expr_expand;
  expandable_class->equals = expr_equals;
  expandable_class->build_string = expr_build_string;
  expandable_class->copy = expr_copy;

  g_value_register_transform_func(COIL_TYPE_EXPR, G_TYPE_STRING,
                                  exprval_to_strval);
}

