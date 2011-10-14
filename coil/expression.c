/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */

#include "common.h"

#include "struct.h"
#include "object.h"
#include "expression.h"

G_DEFINE_TYPE(CoilExpr, coil_expr, COIL_TYPE_OBJECT);

#define COIL_EXPR_GET_PRIVATE(obj) \
    (G_TYPE_INSTANCE_GET_PRIVATE((obj), COIL_TYPE_EXPR, CoilExprPrivate))

struct _CoilExprPrivate
{
    GString   *expr;
    GValue    *expanded_value;
    gboolean   is_expanded : 1;
};

static gboolean
expr_is_expanded(CoilObject *object)
{
    g_return_val_if_fail(COIL_IS_EXPR(object), FALSE);

    return COIL_EXPR(object)->priv->is_expanded;
}

static void
append_path_substitution(CoilObject *self, GString *buffer,
        CoilStringFormat *format, const gchar *path, guint len)
{
    g_return_if_fail(COIL_IS_EXPR(self));
    g_return_if_fail(path != NULL);
    g_return_if_fail(len > 0);

    const GValue *value;

    value = coil_struct_lookup(self->container, path, len, TRUE);
    if (coil_error_occurred())
        return;
    if (value == NULL)
        return;
    coil_value_build_string(value, buffer, format);
}


static gboolean
expr_expand(CoilObject *self, const GValue **return_value)
{
    g_return_val_if_fail(COIL_IS_EXPR(self), FALSE);

    CoilExprPrivate *priv = COIL_EXPR(self)->priv;
    CoilStringFormat format = default_string_format;
    GString *expr = priv->expr, *buffer;
    const gchar *s, *p, *e;

    if (priv->is_expanded) {
        goto done;
    }
    buffer = g_string_sized_new(128);

    format.indent_level = 0;
    format.options &= ~ESCAPE_QUOTES;
    format.options |= DONT_QUOTE_STRINGS;

    e = expr->str + expr->len;
    for (s = expr->str; *s; s++) {
        if (*s == '\\') {
            g_string_append_c(buffer, *++s);
            continue;
        }
        /**
         * TODO(jcon): Add more advanced possibly bash style
         * replacements here as well as list indexing
         */
        if (*s == '$' && s[1] == '{') {
            s += 2;
            /* FIXME: make sure this isnt escaped */
            p = memchr(s + 1, '}', e - s);
            if (p == NULL) {
                coil_object_error(COIL_ERROR_VALUE, self,
                    "Unterminated expression ${%.*s", (int)(p - s), s);
                g_string_free(buffer, TRUE);
                return FALSE;
            }
            append_path_substitution(self, buffer, &format, s, p - s);
            if (coil_error_occurred()) {
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
    if (return_value) {
        *return_value = priv->expanded_value;
    }
    return TRUE;
}

static gboolean
expr_equals(CoilObject *object, CoilObject *other)
{
    g_return_val_if_fail(COIL_IS_EXPR(object), FALSE);
    g_return_val_if_fail(COIL_IS_OBJECT(other), FALSE);

    const GValue *v1, *v2;
    const gchar *s1;
    gchar *s2;
    gboolean result;

    if (!coil_object_expand(object, &v1, TRUE)) {
        return FALSE;
    }
    s1 = g_value_get_string(v1);
    if (!coil_object_expand(other, &v2, TRUE)) {
        return FALSE;
    }
    s2 = g_strdup_value_contents(v2);
    result = g_str_equal(s1, s2);
    g_free(s2);
    return result;
}

static void
expr_build_string(CoilObject *object, GString *buffer,
        CoilStringFormat *format)
{
    g_return_if_fail(COIL_IS_EXPR(object));
    g_return_if_fail(buffer != NULL);
    g_return_if_fail(format != NULL);

    const GValue *return_value = NULL;

    if (!coil_expand((gpointer)object, &return_value, TRUE)) {
        return;
    }
    coil_value_build_string(return_value, buffer, format);
}

static gchar *
expr_to_string(CoilObject *self, CoilStringFormat *format)
{
    g_return_val_if_fail(COIL_IS_EXPR(self), NULL);

    GString *buffer = g_string_sized_new(128);

    expr_build_string(self, buffer, format);

    if (coil_error_occurred()) {
        return NULL;
    }
    return g_string_free(buffer, FALSE);
}

static gboolean
expr_translate_path(GString *expr, CoilObject *old_container,
        CoilObject *new_container)
{
    g_return_val_if_fail(COIL_IS_STRUCT(old_container), FALSE);
    g_return_val_if_fail(COIL_IS_STRUCT(new_container), FALSE);

    guint i;
    const gchar *s, *e;
    CoilPath *path, *new_path;

    for (i = 0, s = expr->str; i < expr->len; i++, s++) {
        if (*s == '\\') {
            s++;
            i++;
            continue;
        }
        if (*s == '$' && s[1] == '{') {
            s += 2;
            i += 2;
            e = memchr(s + 1, '}', e - s);
            if (e == NULL) {
                coil_set_error(COIL_ERROR_VALUE, NULL,
                        "Unterminated expression ${%.*s", (int)(e - s), s);
                return FALSE;
            }
            path = coil_path_new_len(s, e - s);
            if (path == NULL) {
                return FALSE;
            }
            new_path = coil_path_relativize(path, old_container->path);
            g_string_erase(expr, i, path->len);
            g_string_insert_len(expr, i, new_path->str, new_path->len);
            coil_path_unref(path);
            coil_path_unref(new_path);
        }
    }
    return TRUE;
}

static CoilObject *
expr_copy(CoilObject *_self, const gchar *first_property_name,
        va_list properties)
{
    g_return_val_if_fail(COIL_IS_OBJECT(_self), NULL);

    CoilExpr *self = COIL_EXPR(_self);
    CoilExpr *copy;
    CoilExprPrivate *priv = self->priv;
    GString *string;

    string = g_string_new_len(priv->expr->str, priv->expr->len);
    copy = coil_expr_new_valist(string, first_property_name, properties);

    CoilObject *new_container = COIL_OBJECT(copy)->container;
    CoilObject *old_container = COIL_OBJECT(self)->container;

    if (old_container->root != new_container->root &&
            !expr_translate_path(string, old_container, new_container)) {
        return NULL;
    }
    return COIL_OBJECT(copy);
}

static void
exprval_to_strval(const GValue *exprval, GValue *strval)
{
    g_return_if_fail(G_IS_VALUE(exprval));
    g_return_if_fail(G_IS_VALUE(strval));

    CoilObject *obj = COIL_OBJECT(g_value_get_object(exprval));
    gchar *string = expr_to_string(obj, &default_string_format);
    g_value_take_string(strval, string);
}

COIL_API(CoilExpr *)
coil_expr_new_string(const gchar *string, size_t len,
                     const gchar *first_property_name,
                     ...)
{
  CoilExpr *result;
  GString *gstring;
  va_list properties;

  gstring = g_string_new_len(string, len);
  va_start(properties, first_property_name);
  result = coil_expr_new_valist(gstring, first_property_name, properties);
  va_end(properties);

  return result;
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
coil_expr_new_valist(GString *string, const gchar *first_property_name,
        va_list properties)
{
    GObject *object;
    CoilExpr *self;
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
    CoilExpr *const self = COIL_EXPR(object);
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
    CoilObjectClass *object_class;

    gobject_class = G_OBJECT_CLASS(klass);
    object_class = COIL_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(CoilExprPrivate));

    gobject_class->finalize = coil_expr_finalize;

    object_class->is_expanded = expr_is_expanded;
    object_class->expand = expr_expand;
    object_class->equals = expr_equals;
    object_class->build_string = expr_build_string;
    object_class->copy = expr_copy;

    g_value_register_transform_func(COIL_TYPE_EXPR, G_TYPE_STRING,
            exprval_to_strval);
}

