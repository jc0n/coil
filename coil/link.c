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

#define COIL_LINK_GET_PRIVATE(lnk) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((lnk), COIL_TYPE_LINK, CoilLinkPrivate))

struct _CoilLinkPrivate
{
  CoilPath   *path;
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

  CoilLinkPrivate *const priv = self->priv;

  return priv->path;
}

static gboolean
link_is_expanded(gconstpointer link)
{
  return FALSE;
}

static gboolean
link_expand(gconstpointer   link,
            const GValue  **return_value,
            GError        **error)
{
  g_return_val_if_fail(COIL_IS_LINK(link), FALSE);
//  g_return_val_if_fail(return_value == NULL || *return_value == NULL, FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  CoilLink        *const self = COIL_LINK(link);
  CoilStruct      *container = COIL_EXPANDABLE(link)->container;
  CoilPath        *target_path;
  const GValue    *value;
  GError          *internal_error = NULL;

//#ifdef COIL_OLD_LINKS
  const CoilPath  *container_path;
  g_assert(container);
  container_path = coil_struct_get_path(container);
  g_assert(container_path);
  target_path = coil_path_resolve(self->target_path, container_path, error);
//#else
//  CoilLinkPrivate *const priv = self->priv;
//  const CoilPath  *container_path;
//  container_path = coil_struct_get_path(container)
//  target_path = coil_path_resolve(self->target_path, container_path, error);
//#endif

  if (target_path == NULL)
    goto error;

  if (target_path != self->target_path)
  {
    coil_path_unref(self->target_path);
    self->target_path = coil_path_ref(target_path);
  }

  value = coil_struct_lookup_path(container, target_path,
                                  FALSE, &internal_error);

  if (G_UNLIKELY(value == NULL))
  {
    if (internal_error)
      g_propagate_error(error, internal_error);
    else
      coil_link_error(error, self,
          "target path '%s' does not exist.",
          self->target_path->path);

    goto error;
  }

  g_assert(G_IS_VALUE(value));

  if (return_value)
    *return_value = value;

  coil_path_unref(target_path);

  return TRUE;

error:
  if (target_path != NULL)
    coil_path_unref(target_path);

  if (return_value)
    *return_value = NULL;

  return FALSE;
}

COIL_API(gboolean)
coil_link_equals(gconstpointer  self_,
                 gconstpointer  other_,
                 GError       **error)
{
  g_return_val_if_fail(COIL_IS_LINK(self_), FALSE);
  g_return_val_if_fail(COIL_IS_LINK(other_), FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  const CoilLink *self = COIL_LINK(self_);
  const CoilLink *other = COIL_LINK(other_);

  if (self == other)
    return TRUE;

  // Check disjoint roots
  const CoilStruct *self_ctnr = COIL_EXPANDABLE(self)->container;
  const CoilStruct *other_ctnr = COIL_EXPANDABLE(self)->container;

  if (coil_struct_has_same_root(self_ctnr, other_ctnr))
    return coil_path_equal(self->target_path, other->target_path);

  const GValue *self_value = NULL;
  const GValue *other_value = NULL;

  if (!coil_expand(COIL_EXPANDABLE(self), &self_value, FALSE, error)
    || !coil_expand(COIL_EXPANDABLE(other), &other_value, FALSE, error))
    return FALSE;

  g_assert(G_IS_VALUE(self_value));
  g_assert(G_IS_VALUE(other_value));

  return coil_value_compare(self_value, other_value, error) == 0;
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

  const CoilPath *container_path;
  CoilPath       *target_path;
  CoilStruct     *container = COIL_EXPANDABLE(self)->container;

  container_path = coil_struct_get_path(container);

  if (format->options & FLATTEN_PATHS)
    target_path = coil_path_resolve(self->target_path, container_path, NULL);
  else
    target_path = coil_path_relativize(self->target_path, container_path);

  if (G_LIKELY(target_path))
  {
    g_string_append_printf(buffer, "=%s", target_path->path);
    coil_path_unref(target_path);
  }
  else
    g_string_append_printf(buffer, "=%s", self->target_path->path);
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
coil_link_to_string(CoilLink         *self,
                    CoilStringFormat *format)
{
  g_return_val_if_fail(COIL_IS_LINK(self), NULL);
  g_return_val_if_fail(self->target_path, NULL);

  GString *buffer = g_string_sized_new(128);
  coil_link_build_string(self, buffer, format);

  return g_string_free(buffer, FALSE);
}

static void
linkval_to_stringval(const GValue *linkval,
                           GValue *strval)
{
  g_return_if_fail(G_IS_VALUE(linkval));
  g_return_if_fail(G_IS_VALUE(strval));

  CoilLink *link;
  gchar    *string;

  link = COIL_LINK(g_value_get_object(linkval));
  string = coil_link_to_string(link, &default_string_format);
  g_value_take_string(strval, string);
}

CoilLink *
coil_link_new(GError **error,
              const gchar *first_property_name,
              ...)
{
  va_list          args;
  GObject         *object;
  CoilLink        *self;
  CoilLinkPrivate *priv;

  va_start(args, first_property_name);
  object = g_object_new_valist(COIL_TYPE_LINK, first_property_name, args);
  va_end(args);

  self = COIL_LINK(object);
  priv = self->priv;

  g_assert(priv);

  if (self->target_path == NULL)
    g_error("Link must be constructed with a path.");

  if (COIL_PATH_IS_ROOT(self->target_path))
  {
    coil_link_error(error, self,
                    "Cannot link to root path");

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

  switch (property_id)
  {
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

  switch (property_id)
  {
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

  g_value_register_transform_func(COIL_TYPE_LINK,
                                  G_TYPE_STRING,
                                  linkval_to_stringval);
}

