/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */
#include "common.h"

#include "struct.h"
#include "path.h"
#include "value.h"
#include "link.h"

G_DEFINE_TYPE(CoilLink, coil_link, COIL_TYPE_OBJECT);

#define COIL_LINK_GET_PRIVATE(o) \
    (G_TYPE_INSTANCE_GET_PRIVATE((o), COIL_TYPE_LINK, CoilLinkPrivate))

struct _CoilLinkPrivate
{
    CoilPath *target;
};

typedef enum
{
    PROP_0,
    PROP_TARGET,
} LinkProperties;

static void
coil_link_error(CoilObject *self, const char *format, ...)
{
    g_return_if_fail(self);
    g_return_if_fail(format);

    va_list args;

    va_start(args, format);
    coil_object_error_valist(COIL_ERROR_LINK, self, format, args);
    va_end(args);
}

static gboolean
link_is_expanded(CoilObject *link)
{
    return FALSE;
}

static const GValue *
lookup_target_value(CoilObject *self)
{
    CoilLinkPrivate *priv = COIL_LINK(self)->priv;
    CoilObject *container = self->container;
    CoilPath *path = NULL, *structpath = NULL, *relpath = NULL;
    const GValue *value = NULL, *structval;

    path = coil_path_resolve(priv->target, container->path);
    if (path == NULL)
        goto end;
    value = coil_struct_lookupx(container, path, TRUE);
    if (value) {
        /* fast-path: entry found by direct lookup */
        goto end;
    }

    structpath = coil_path_pop(path, 1);
    if (structpath == NULL)
        goto end;
    structval = coil_struct_lookupx(self->root, structpath, TRUE);
    if (structval == NULL)
        goto end;

    container = COIL_OBJECT(g_value_dup_object(structval));
    relpath = coil_path_relativize(path, structpath);
    if (relpath == NULL) {
        coil_object_unref(container);
        goto end;
    }
    value = coil_struct_lookupx(container, relpath, TRUE);
    coil_object_unref(container);
end:
    if (value == NULL) {
        coil_link_error(self, "link target path '%s' not found.", path->str);
    }
    coil_path_unrefx(structpath);
    coil_path_unrefx(relpath);
    coil_path_unrefx(path);
    return value;
}

static gboolean
link_expand(CoilObject *self, const GValue **return_value)
{
    const GValue *value;

    value = lookup_target_value(self);
    if (value) {
        *return_value = value;
        return TRUE;
    }
    *return_value = NULL;
    return FALSE;
}

static gboolean
link_set_target(CoilObject *self, CoilPath *target)
{
    g_return_val_if_fail(self, FALSE);
    g_return_val_if_fail(target, FALSE);

    CoilLinkPrivate *priv = COIL_LINK(self)->priv;

    if (COIL_PATH_IS_ROOT(target)) {
        coil_link_error(self, "Cannot link to @root");
        return FALSE;
    }
    if (priv->target) {
        coil_path_unref(priv->target);
    }
    priv->target = coil_path_ref(target);
    return TRUE;
}

static void
link_set_container(CoilObject *self, CoilObject *container)
{
    g_return_if_fail(self);

    if (container) {
        CoilLinkPrivate *priv = COIL_LINK(self)->priv;
        /* Note: If the target path is absolute and the root changes, the path becomes invalid. */
        if (priv->target != NULL && self->container != NULL &&
            COIL_PATH_IS_ABSOLUTE(priv->target) &&
            container->root != self->root) {
            /* path is relativized against old container so it
             * applies in the new container. */
            CoilPath *path = coil_path_relativize(priv->target, self->container->path);
            link_set_target(self, path);
            coil_path_unref(path);
        }
    }
}

static CoilObject *
link_copy(CoilObject *self, const gchar *first_property_name, va_list properties)
{
    g_return_val_if_fail(COIL_IS_LINK(self), NULL);

    CoilLinkPrivate *priv = COIL_LINK(self)->priv;
    CoilPath *path = priv->target;

    if (self->container && self->container->path) {
        path = coil_path_relativize(path, self->container->path);
        if (path == NULL) {
            return NULL;
        }
    }
    return coil_link_new_valist(path, first_property_name, properties);
}

static gboolean
link_equals(CoilObject *self, CoilObject *other)
{
    const GValue *selfval, *otherval;

    if (self == other)
        return TRUE;

    if (!coil_object_expand(self, &selfval, FALSE))
        return FALSE;

    if (!coil_object_expand(other, &otherval, FALSE))
        return FALSE;

    return coil_value_compare(selfval, otherval) == 0;
}

static void
link_build_string(CoilObject *self, GString *buffer, CoilStringFormat *format)
{
    g_return_if_fail(COIL_IS_LINK(self));
    g_return_if_fail(buffer);
    g_return_if_fail(format);

    CoilLinkPrivate *priv = COIL_LINK(self)->priv;
    CoilPath *path;

    if (format->options & FLATTEN_PATHS) {
        path = coil_path_resolve(priv->target, self->container->path);
    }
    else {
        path = coil_path_relativize(priv->target, self->container->path);
    }
    if (path) {
        g_string_append_printf(buffer, "=%s", path->str);
        coil_path_unref(path);
    }
}

static gchar *
link_to_string(CoilObject *link, CoilStringFormat *format)
{
    g_return_val_if_fail(COIL_IS_LINK(link), NULL);

    GString *buffer = g_string_sized_new(128);
    link_build_string(link, buffer, format);

    return g_string_free(buffer, FALSE);
}

static void
linkval_to_stringval(const GValue *linkval, GValue *strval)
{
    g_return_if_fail(G_IS_VALUE(linkval));
    g_return_if_fail(G_IS_VALUE(strval));

    CoilObject *link = COIL_OBJECT(g_value_get_object(linkval));
    gchar *string = link_to_string(link, &default_string_format);

    g_value_take_string(strval, string);
}

COIL_API(CoilObject *)
coil_link_new(CoilPath *target, const gchar *first_property_name, ...)
{
    va_list properties;
    CoilObject *link;

    va_start(properties, first_property_name);
    link = coil_link_new_valist(target, first_property_name, properties);
    va_end(properties);

    return link;
}

COIL_API(CoilObject *)
coil_link_new_valist(CoilPath *target, const gchar *first_property_name, va_list properties)
{
    CoilObject *self = COIL_OBJECT(g_object_new_valist(COIL_TYPE_LINK,
                first_property_name, properties));

    if (!link_set_target(self, target)) {
        coil_object_unref(self);
        return NULL;
    }
    return self;
}

static void
coil_link_finalize(GObject *object)
{
    CoilLinkPrivate *priv = COIL_LINK(object)->priv;

    if (priv->target) {
        coil_path_unref(priv->target);
    }
    G_OBJECT_CLASS(coil_link_parent_class)->finalize(object);
}

static void
coil_link_get_property(GObject    *object,
                       guint       property_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
    CoilLinkPrivate *priv = COIL_LINK(object)->priv;

    switch (property_id) {
        case PROP_TARGET:
            g_value_set_boxed(value, priv->target);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
coil_link_init(CoilLink *self)
{
    CoilLinkPrivate *priv = COIL_LINK_GET_PRIVATE(self);
    self->priv = priv;
}

static void
coil_link_class_init(CoilLinkClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    CoilObjectClass *object_class = COIL_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(CoilLinkPrivate));

    gobject_class->get_property = coil_link_get_property;
    gobject_class->finalize = coil_link_finalize;

    object_class->is_expanded = link_is_expanded;
    object_class->copy = link_copy;
    object_class->expand = link_expand;
    object_class->equals = link_equals;
    object_class->build_string = link_build_string;
    object_class->set_container = link_set_container;

    g_object_class_install_property(gobject_class, PROP_TARGET,
            g_param_spec_boxed("target",
                "The target path to where the link points.",
                "set/get the path where the link points.",
                COIL_TYPE_PATH,
                G_PARAM_READABLE));

    g_value_register_transform_func(COIL_TYPE_LINK, G_TYPE_STRING,
            linkval_to_stringval);
}

