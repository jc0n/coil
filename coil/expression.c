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
append_path_substitution(CoilExpr *self, GString *buffer,
        CoilStringFormat *format, const gchar *path, guint len, GError **error)
{
    g_return_if_fail(COIL_IS_EXPR(self));
    g_return_if_fail(path != NULL);
    g_return_if_fail(len > 0);

    CoilObject *obj = COIL_OBJECT(self);
    const GValue *value;
    GError *internal_error = NULL;

    value = coil_struct_lookup(obj->container, path, len, TRUE, &internal_error);

    if (G_UNLIKELY(internal_error)) {
        g_propagate_error(error, internal_error);
        return;
    }

    if (value == NULL)
        return;

    coil_value_build_string(value, buffer, format, NULL);
}


static gboolean
expr_expand(CoilObject *object, const GValue **return_value, GError **error)
{
    g_return_val_if_fail(COIL_IS_EXPR(object), FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    CoilExpr *self = COIL_EXPR(object);
    CoilExprPrivate *const priv = self->priv;
    CoilStringFormat format = default_string_format;
    GString *expr = priv->expr, *buffer;
    const gchar *s, *p;
    GError *internal_error = NULL;

    if (priv->is_expanded) {
        goto done;
    }

    buffer = g_string_sized_new(128);

    format.indent_level = 0;
    format.options &= ~ESCAPE_QUOTES;
    format.options |= DONT_QUOTE_STRINGS;

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
            /* XXX: safe b.c lexer has already found '}' */
            p = rawmemchr(s + 1, '}');
            append_path_substitution(self, buffer, &format, s, p - s, &internal_error);
            if (G_UNLIKELY(internal_error)) {
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
    if (return_value) {
        *return_value = priv->expanded_value;
    }
    return TRUE;
}

static gboolean
expr_equals(CoilObject *object, CoilObject *other, GError **error)
{
    g_return_val_if_fail(COIL_IS_EXPR(object), FALSE);
    g_return_val_if_fail(COIL_IS_OBJECT(other), FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    const GValue *v1, *v2;
    const gchar *s1;
    gchar *s2;
    gboolean result;

    if (!coil_object_expand(object, &v1, TRUE, error)) {
        return FALSE;
    }
    s1 = g_value_get_string(v1);
    if (!coil_object_expand(other, &v2, TRUE, error)) {
        return FALSE;
    }
    s2 = g_strdup_value_contents(v2);
    result = g_str_equal(s1, s2);
    g_free(s2);
    return result;
}

static void
expr_build_string(CoilObject *object, GString *buffer,
        CoilStringFormat *format, GError **error)
{
    g_return_if_fail(COIL_IS_EXPR(object));
    g_return_if_fail(buffer != NULL);
    g_return_if_fail(format != NULL);
    g_return_if_fail(error == NULL || *error == NULL);

    const GValue *return_value = NULL;

    if (!coil_expand((gpointer)object, &return_value, TRUE, error)) {
        return;
    }
    coil_value_build_string(return_value, buffer, format, error);
}

static gchar *
expr_to_string(CoilObject *self, CoilStringFormat *format, GError **error)
{
    g_return_val_if_fail(COIL_IS_EXPR(self), NULL);
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    GString *buffer = g_string_sized_new(128);
    GError *internal_error = NULL;

    expr_build_string(self, buffer, format, &internal_error);

    if (G_UNLIKELY(internal_error)) {
        g_propagate_error(error, internal_error);
        return NULL;
    }
    return g_string_free(buffer, FALSE);
}

#if COIL_PATH_TRANSLATION
static gboolean
expr_translate_path(GString *expr, CoilObject *old_container,
        CoilObject *new_container, GError **error)
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
            e = rawmemchr(s + 1, '}');
            path = coil_path_new_len(s, e - s, error);
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
#endif

static CoilObject *
expr_copy(CoilObject *_self, const gchar *first_property_name,
        va_list properties, GError **error)
{
    g_return_val_if_fail(COIL_IS_OBJECT(_self), NULL);
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    CoilExpr *self = COIL_EXPR(_self);
    CoilExpr *copy;
    CoilExprPrivate *priv = self->priv;
    GString *string;

    string = g_string_new_len(priv->expr->str, priv->expr->len);
    copy = coil_expr_new_valist(string, first_property_name, properties);

#if COIL_PATH_TRANSLATION
    CoilObject *new_container = COIL_OBJECT(copy)->container;
    CoilObject *old_container = COIL_OBJECT(self)->container;

    if (old_container->root != new_container->root &&
            !expr_translate_path(string, old_container, new_container, error)) {
        return NULL;
    }
#endif
    return COIL_OBJECT(copy);
}

static void
exprval_to_strval(const GValue *exprval, GValue *strval)
{
    g_return_if_fail(G_IS_VALUE(exprval));
    g_return_if_fail(G_IS_VALUE(strval));

    CoilObject *obj = COIL_OBJECT(g_value_get_object(exprval));
    GError *internal_error = NULL;
    gchar *string = expr_to_string(obj, &default_string_format, &internal_error);

    if (G_UNLIKELY(internal_error)) {
        g_warning("%s: %s", G_STRLOC, internal_error->message);
        g_error_free(internal_error);
        return;
    }
    g_value_take_string(strval, string);
}

COIL_API(CoilExpr *)
coil_expr_new(GString *string, const gchar *first_property_name, ...)
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

