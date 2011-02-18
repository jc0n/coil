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
append_path_substitution(CoilExpr    *self,
                  GString     *buffer,
                  const gchar *path,
                  guint        len)
{
  g_return_if_fail(COIL_IS_EXPR(self));
  g_return_if_fail(path != NULL);
  g_return_if_fail(len > 0);

  CoilStruct      *container = COIL_EXPANDABLE(self)->container;
  const GValue    *value;

  value = coil_struct_lookup(container, path, len, TRUE, NULL);

  if (value == NULL)
    return;

  coil_value_build_string(value, buffer, NULL);
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
  GString         *expr = priv->expr, *buffer;
  const gchar     *s, *p;

  if (priv->is_expanded)
    goto done;

  buffer = g_string_sized_new(128);

  for (s = p = expr->str; *s; s++)
  {
    if (*s == '\\')
    {
      s++;
      continue;
    }

    /**
     * TODO(jcon): Add more advanced possibly bash style
     * replacements here as well as list indexing
     */
    if (*s == '$' && *++s == '{')
    {
      /* copy literal into buffer */
      g_string_append_len(buffer, p, s - p - 1);

      p = rawmemchr(++s, '}'); /* XXX: safe b.c lexer has already found '}' */
      append_path_substitution(self, buffer, s, p - s);
      p++;
    }
  }

  g_string_append(buffer, p);

  new_value(priv->expanded_value, G_TYPE_STRING,
            take_string, g_string_free(buffer, FALSE));

  g_string_free(priv->expr, TRUE);
  priv->expr = NULL;

  priv->is_expanded = TRUE;

done:
  if (return_value)
    *return_value = priv->expanded_value;

  return TRUE;
}

static gint
expr_equals(gconstpointer  object,
            gconstpointer  other_object,
            GError       **error)
{
  g_return_val_if_fail(COIL_IS_EXPR(object), -2);
  g_return_val_if_fail(other_object != NULL, -2);
  g_return_val_if_fail(error == NULL || *error == NULL, -2);

  const GValue  *v1, *v2;
  const GString *s1, *s2;

  if (!COIL_IS_EXPR(other_object))
    return -2;

  if (!coil_expand((gpointer)object, &v1, TRUE, error))
    return -2;

  if (!coil_expand((gpointer)other_object, &v2, TRUE, error))
    return -2;

  s1 = (GString *)g_value_get_boxed(v1);
  s2 = (GString *)g_value_get_boxed(v2);

  if (s1->len != s2->len)
    return s1->len > s2->len ? 1 : -1;

  return memcmp(s1->str, s2->str, s2->len);
}

static void
expr_build_string(gconstpointer object,
                  GString      *buffer,
                  GError      **error)
{
  g_return_if_fail(object != NULL);
  g_return_if_fail(buffer != NULL);
  g_return_if_fail(error == NULL || *error == NULL);

  const GValue *return_value = NULL;

  coil_expand((gpointer)object, &return_value, TRUE, error);

  coil_value_build_string(return_value, buffer, error);
}

static gchar *
coil_expr_to_string(CoilExpr *self,
                    GError  **error)
{
  g_return_val_if_fail(COIL_IS_EXPR(self), NULL);
  g_return_val_if_fail(error == NULL || *error == NULL, NULL);

  GString *buffer = g_string_sized_new(128);
  GError  *internal_error = NULL;

  expr_build_string(self, buffer, &internal_error);

  if (G_UNLIKELY(internal_error))
    return NULL;

  return g_string_free(buffer, FALSE);
}

static void
exprval_to_strval(const GValue *exprval,
                        GValue *strval)
{
  g_return_if_fail(G_IS_VALUE(exprval));
  g_return_if_fail(G_IS_VALUE(strval));

  CoilExpr *expr = COIL_EXPR(g_value_get_object(exprval));
  gchar    *string = coil_expr_to_string(expr, NULL);

  g_value_take_string(strval, string);
}

CoilExpr *
coil_expr_new(GString *string)
{
  GObject         *object;
  CoilExpr        *self;
  CoilExprPrivate *priv;

  object = g_object_new(COIL_TYPE_EXPR, NULL);
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
    free_value(priv->expanded_value);

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

  g_value_register_transform_func(COIL_TYPE_EXPR, G_TYPE_STRING,
                                  exprval_to_strval);
}

