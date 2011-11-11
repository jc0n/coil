/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */
#include "common.h"

#include <string.h>

#include "struct.h"
#include "path.h"
#include "value.h"
#include "link.h"

G_DEFINE_TYPE(CoilLink, coil_link, COIL_TYPE_OBJECT);

typedef enum
{
    PROP_0,
    PROP_TARGET_PATH,
} LinkProperties;

static gboolean
link_is_expanded(CoilObject *link)
{
    return FALSE;
}

static gboolean
link_expand(CoilObject *o, const GValue **return_value, GError **error)
{
    g_return_val_if_fail(COIL_IS_LINK(o), FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    CoilLink *self = COIL_LINK(o);
    CoilObject *container = o->container;
    const GValue *value;
    GError *internal_error = NULL;

    if (!coil_path_resolve_inplace(&self->target_path, container->path, error)) {
        goto error;
    }
    value = coil_struct_lookupx(container, self->target_path,
            FALSE, &internal_error);

    if (G_UNLIKELY(value == NULL)) {
        if (internal_error) {
            g_propagate_error(error, internal_error);
        }
        else {
            coil_link_error(error, o,
                    "target path '%s' does not exist.",
                    self->target_path->str);
        }
        goto error;
    }
    g_assert(G_IS_VALUE(value));
    if (return_value)
        *return_value = value;

    return TRUE;

error:
    if (return_value)
        *return_value = NULL;

    return FALSE;
}

static CoilObject *
link_copy(CoilObject *obj, const gchar *first_property_name,
        va_list properties, GError **error)
{
    g_return_val_if_fail(COIL_IS_LINK(obj), NULL);
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    CoilLink *self = COIL_LINK(obj);
    CoilObject *copy = coil_link_new(error, "target_path", self->target_path, NULL);
    CoilPath *target_path = NULL;

    if (copy == NULL)
        return NULL;

    g_object_set_valist(G_OBJECT(copy), first_property_name, properties);
    coil_object_get(copy, "target_path", &target_path, NULL);

#if COIL_PATH_TRANSLATION
    if (COIL_PATH_IS_ABSOLUTE(target_path)) {
        CoilObject *new_container = copy->container;
        CoilObject *old_container = COIL_OBJECT(self)->container;

        if (old_container->root != new_container->root) {
            CoilPath *path;

            path = coil_path_relativize(target_path, old_container->path);
            coil_object_set(copy, "target_path", path, NULL);
            coil_path_unref(path);
        }
    }
#endif
    return COIL_OBJECT(copy);
}

static gboolean
link_equals(CoilObject *self, CoilObject *other, GError **error)
{
    g_return_val_if_fail(COIL_IS_LINK(self), FALSE);
    g_return_val_if_fail(COIL_IS_LINK(other), FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    const GValue *selfval, *otherval;

    if (self == other)
        return TRUE;

    if (!coil_object_expand(self, &selfval, FALSE, error))
        return FALSE;

    if (!coil_object_expand(other, &otherval, FALSE, error))
        return FALSE;

    return coil_value_compare(selfval, otherval, error) == 0;
}

static void
link_build_string(CoilObject *link, GString *buffer,
        CoilStringFormat *format, GError **error)
{
    g_return_if_fail(COIL_IS_LINK(link));
    g_return_if_fail(buffer);
    g_return_if_fail(format);

    CoilPath *target_path;
    CoilLink *self = COIL_LINK(link);
    CoilObject *obj = COIL_OBJECT(self);
    CoilPath *container_path = obj->container->path;

    if (format->options & FLATTEN_PATHS)
        target_path = coil_path_resolve(self->target_path, container_path, NULL);
    else
        target_path = coil_path_relativize(self->target_path, container_path);

    if (G_LIKELY(target_path)) {
        g_string_append_printf(buffer, "=%s", target_path->str);
        coil_path_unref(target_path);
    }
    else {
        g_string_append_printf(buffer, "=%s", self->target_path->str);
    }
}

static gchar *
link_to_string(CoilObject *link, CoilStringFormat *format)
{
    g_return_val_if_fail(COIL_IS_LINK(link), NULL);

    GString *buffer = g_string_sized_new(128);
    link_build_string(link, buffer, format, NULL);

    return g_string_free(buffer, FALSE);
}

static void
linkval_to_stringval(const GValue *linkval,
                           GValue *strval)
{
    g_return_if_fail(G_IS_VALUE(linkval));
    g_return_if_fail(G_IS_VALUE(strval));

    CoilObject *link;
    gchar    *string;

    link = COIL_OBJECT(g_value_get_object(linkval));
    string = link_to_string(link, &default_string_format);
    g_value_take_string(strval, string);
}

COIL_API(CoilObject *)
coil_link_new(GError **error,
              const gchar *first_property_name,
              ...)
{
    va_list   properties;
    CoilObject *link;

    va_start(properties, first_property_name);
    link = coil_link_new_valist(first_property_name, properties, error);
    va_end(properties);

    return link;
}

COIL_API(CoilObject *)
coil_link_new_valist(const gchar *first_property_name,
                     va_list      properties,
                     GError     **error)
{
    CoilObject *object;
    CoilLink *self;

    object = COIL_OBJECT(g_object_new_valist(COIL_TYPE_LINK,
                first_property_name, properties));
    self = COIL_LINK(object);

    if (self->target_path == NULL) {
        g_error("Link must be constructed with a path.");
    }
    if (COIL_PATH_IS_ROOT(self->target_path)) {
        coil_link_error(error, object, "Cannot link to root path");
        return NULL;
    }
    return COIL_OBJECT(self);
}

static void
coil_link_finalize(GObject *object)
{
    CoilLink *self = COIL_LINK(object);

    if (self->target_path) {
        coil_path_unref(self->target_path);
    }

    G_OBJECT_CLASS(coil_link_parent_class)->finalize(object);
}

static void
coil_link_set_property(GObject      *object,
                       guint         property_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
    CoilLink *self = COIL_LINK(object);

    switch (property_id) {
        case PROP_TARGET_PATH:
            if (self->target_path)
                coil_path_unref(self->target_path);
            self->target_path = g_value_dup_boxed(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
coil_link_get_property(GObject    *object,
                         guint       property_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
    CoilLink *self = COIL_LINK(object);

    switch (property_id) {
        case PROP_TARGET_PATH:
            g_value_set_boxed(value, self->target_path);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
coil_link_init(CoilLink *self)
{
}

static void
coil_link_class_init(CoilLinkClass *klass)
{
    GObjectClass *gobject_class;
    CoilObjectClass *object_class;

    gobject_class = G_OBJECT_CLASS(klass);
    object_class = COIL_OBJECT_CLASS(klass);

    gobject_class->set_property = coil_link_set_property;
    gobject_class->get_property = coil_link_get_property;
    gobject_class->finalize = coil_link_finalize;

    object_class->is_expanded = link_is_expanded;
    object_class->copy = link_copy;
    object_class->expand = link_expand;
    object_class->equals = link_equals;
    object_class->build_string = link_build_string;

    g_object_class_install_property(gobject_class, PROP_TARGET_PATH,
            g_param_spec_boxed("target_path",
                "The path the link points to.",
                "set/get the path the link points to.",
                COIL_TYPE_PATH,
                G_PARAM_READWRITE |
                G_PARAM_CONSTRUCT));

    g_value_register_transform_func(COIL_TYPE_LINK, G_TYPE_STRING,
            linkval_to_stringval);
}

