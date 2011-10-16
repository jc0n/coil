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

G_DEFINE_TYPE(CoilLink, coil_link, COIL_TYPE_EXPANDABLE);

#define COIL_LINK_GET_PRIVATE(lnk)                                         \
    (G_TYPE_INSTANCE_GET_PRIVATE ((lnk), COIL_TYPE_LINK, CoilLinkPrivate)) \

struct _CoilLinkPrivate
{
    CoilPath *path;
};

typedef enum
{
  PROP_0,
  PROP_PATH,
  PROP_TARGET_PATH,
} LinkProperties;


const CoilPath *
coil_link_get_path(const CoilLink *self)
{
    g_return_val_if_fail(COIL_IS_LINK(self), NULL);
    return self->priv->path;
}

static gboolean
link_is_expanded(gconstpointer link)
{
    return FALSE;
}

static gboolean
link_expand(gconstpointer link,
            const GValue **return_value,
            GError **error)
{
    g_return_val_if_fail(COIL_IS_LINK(link), FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    CoilLink *const self = COIL_LINK(link);
    CoilStruct *container = COIL_EXPANDABLE(link)->container;
    GError *internal_error = NULL;
    const GValue *value;

    value = coil_struct_lookup_path(container, self->target_path,
        FALSE, &internal_error);

    if (value == NULL) {
        if (internal_error != NULL) {
          g_propagate_error(error, internal_error);
        }
        else {
            coil_link_error(error, self,
                    "target path '%s' does not exist, resolving from '%s'",
                    self->target_path->path,
                    coil_struct_get_path(container)->path);
        }
        if (return_value)
            *return_value = NULL;
        return FALSE;
    }

    if (return_value) {
        g_return_val_if_fail(G_IS_VALUE(value), FALSE);
        *return_value = value;
    }
    return TRUE;
}

static CoilExpandable *
link_copy(gconstpointer obj,
          const gchar  *first_property_name,
          va_list       properties,
          GError      **error)
{
    g_return_val_if_fail(COIL_IS_LINK(obj), NULL);
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    CoilLink *self, *copy;

    self = COIL_LINK(obj);
    copy = coil_link_new(error, "target_path", self->target_path, NULL);
    if (copy == NULL)
        return NULL;

    g_object_set_valist(G_OBJECT(copy), first_property_name, properties);

#if COIL_PATH_TRANSLATION
    if (COIL_PATH_IS_ABSOLUTE(copy->target_path)) {
        CoilStruct *newct, *oldct;
        const CoilPath *ctpath;
        CoilPath *path;

        newct = COIL_EXPANDABLE(copy)->container;
        oldct = COIL_EXPANDABLE(self)->container;

        if (!coil_struct_compare_root(oldct, newct)) {
            ctpath = coil_struct_get_path(oldct);
            path = coil_path_relativize(copy->target_path, ctpath);
            coil_path_unref(copy->target_path);
            copy->target_path = path;
        }
    }
#endif
    return COIL_EXPANDABLE(copy);
}

COIL_API(gboolean)
coil_link_equals(gconstpointer  self,
                 gconstpointer  other,
                 GError       **error)
{
    g_return_val_if_fail(COIL_IS_LINK(self), FALSE);
    g_return_val_if_fail(COIL_IS_LINK(other), FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    CoilLink         *a, *b;
    const GValue     *av, *bv;

    a = COIL_LINK(self);
    b = COIL_LINK(other);

    if (a == b)
        return TRUE;

    if (!coil_expand(a, &av, FALSE, error))
        return FALSE;

    if (!coil_expand(b, &bv, FALSE, error))
        return FALSE;

    return coil_value_compare(av, bv, error) == 0;
}

COIL_API(void)
coil_link_build_string(CoilLink         *self,
                       GString          *const buffer,
                       CoilStringFormat *format)
{
    g_return_if_fail(COIL_IS_LINK(self));
    g_return_if_fail(buffer);
    g_return_if_fail(format);
    g_return_if_fail(self->target_path);

    const CoilPath *ctpath;
    CoilPath       *target;
    CoilStruct     *container = COIL_EXPANDABLE(self)->container;

    ctpath = coil_struct_get_path(container);

    if (format->options & FLATTEN_PATHS)
        target = coil_path_resolve(self->target_path, ctpath, NULL);
    else
        target = coil_path_relativize(self->target_path, ctpath);

    if (target == NULL)
        target = coil_path_ref(self->target_path);

    g_string_append_printf(buffer, "=%s", target->path);
    coil_path_unref(target);
}

static void
link_build_string(gconstpointer     link,
                  GString          *const buffer,
                  CoilStringFormat *format,
                  GError          **error /* ignored */)
{
    g_return_if_fail(COIL_IS_LINK(link));
    g_return_if_fail(buffer);
    g_return_if_fail(format);

    coil_link_build_string(COIL_LINK(link), buffer, format);
}

COIL_API(gchar *)
coil_link_to_string(CoilLink *self, CoilStringFormat *format)
{
    g_return_val_if_fail(COIL_IS_LINK(self), NULL);
    g_return_val_if_fail(self->target_path, NULL);

    GString *buffer;

    buffer = g_string_sized_new(128);
    coil_link_build_string(self, buffer, format);

    return g_string_free(buffer, FALSE);
}

static void
linkval_to_stringval(const GValue *linkval, GValue *strval)
{
    g_return_if_fail(G_IS_VALUE(linkval));
    g_return_if_fail(G_IS_VALUE(strval));

    CoilLink *link;
    gchar    *string;

    link = COIL_LINK(g_value_get_object(linkval));
    string = coil_link_to_string(link, &default_string_format);
    g_value_take_string(strval, string);
}

COIL_API(CoilLink *)
coil_link_new(GError **error,
              const gchar *first_property_name,
              ...)
{
    va_list   properties;
    CoilLink *link;

    va_start(properties, first_property_name);
    link = coil_link_new_valist(first_property_name, properties, error);
    va_end(properties);

    return link;
}

COIL_API(CoilLink *)
coil_link_new_valist(const gchar *first_property_name,
                     va_list      properties,
                     GError     **error)
{
    GObject         *object;
    CoilLink        *self;
    CoilLinkPrivate *priv;

    object = g_object_new_valist(COIL_TYPE_LINK, first_property_name, properties);
    self = COIL_LINK(object);
    priv = self->priv;

    if (self->target_path == NULL)
        g_error("Link must be constructed with a path.");

    if (COIL_PATH_IS_ROOT(self->target_path)) {
        coil_link_error(error, self, "Cannot link to root path");
        return NULL;
    }
    return self;
}

static void
coil_link_finalize(GObject *object)
{
    CoilLink        *const self = COIL_LINK(object);
    CoilLinkPrivate *const priv = self->priv;

    if (self->target_path)
        coil_path_unref(self->target_path);

    if (priv->path)
        coil_path_unref(priv->path);

    G_OBJECT_CLASS(coil_link_parent_class)->finalize(object);
}

static void
coil_link_set_property(GObject      *object,
                       guint         property_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
  CoilLink        *const self = COIL_LINK(object);
  CoilLinkPrivate *const priv = self->priv;

  switch (property_id) {
      case PROP_TARGET_PATH:
          if (self->target_path)
              coil_path_unref(self->target_path);
          self->target_path = g_value_dup_boxed(value);
          break;

      case PROP_PATH:
          if (priv->path)
              coil_path_unref(priv->path);
          priv->path = g_value_dup_boxed(value);
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
    CoilLink        *const self = COIL_LINK(object);
    CoilLinkPrivate *const priv = self->priv;

    switch (property_id) {
        case PROP_TARGET_PATH:
            g_value_set_boxed(value, self->target_path);
            break;

        case PROP_PATH:
            g_value_set_boxed(value, priv->path);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
coil_link_init(CoilLink *self)
{
    self->priv = COIL_LINK_GET_PRIVATE(self);
}

static void
coil_link_class_init(CoilLinkClass *klass)
{
    GObjectClass        *gobject_class;
    CoilExpandableClass *expandable_class;

    g_type_class_add_private(klass, sizeof(CoilLinkPrivate));

    gobject_class = G_OBJECT_CLASS(klass);
    expandable_class = COIL_EXPANDABLE_CLASS(klass);

    gobject_class->set_property = coil_link_set_property;
    gobject_class->get_property = coil_link_get_property;
    gobject_class->finalize = coil_link_finalize;

    expandable_class->is_expanded = link_is_expanded;
    expandable_class->copy = link_copy;
    expandable_class->expand = link_expand;
    expandable_class->equals = coil_link_equals;
    expandable_class->build_string = link_build_string;

    g_object_class_install_property(gobject_class, PROP_TARGET_PATH,
            g_param_spec_boxed("target_path",
                "The path the link points to.",
                "set/get the path the link points to.",
                COIL_TYPE_PATH,
                G_PARAM_READWRITE |
                G_PARAM_CONSTRUCT));

    g_object_class_install_property(gobject_class, PROP_PATH,
            g_param_spec_boxed("path",
                "path of the link.",
                "set/get the path of the link.",
                COIL_TYPE_PATH,
                G_PARAM_READWRITE |
                G_PARAM_CONSTRUCT));

    g_value_register_transform_func(COIL_TYPE_LINK, G_TYPE_STRING,
            linkval_to_stringval);
}

