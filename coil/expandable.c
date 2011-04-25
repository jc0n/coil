/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */

#include <string.h>

#include "common.h"
#include "struct.h"
#include "link.h"

G_DEFINE_ABSTRACT_TYPE(CoilExpandable, coil_expandable, G_TYPE_OBJECT);

#define COIL_EXPANDABLE_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE((obj), COIL_TYPE_EXPANDABLE, \
                               CoilExpandablePrivate))

struct _CoilExpandablePrivate
{
  GStaticMutex  expand_lock;
};

typedef enum
{
  PROP_O,
  PROP_CONTAINER,
  PROP_LOCATION,
} CoilExpandableProperties;

COIL_API(void)
coil_expandable_build_string(CoilExpandable   *self,
                             GString          *const buffer,
                             CoilStringFormat *format,
                             GError          **error)
{
  g_return_if_fail(COIL_IS_EXPANDABLE(self));
  g_return_if_fail(error == NULL || *error == NULL);
  g_return_if_fail(format);

  CoilExpandableClass *klass = COIL_EXPANDABLE_GET_CLASS(self);
  return klass->build_string(self, buffer, format, error);
}

COIL_API(gchar *)
coil_expandable_to_string(CoilExpandable   *self,
                          CoilStringFormat *format,
                          GError          **error)
{
  g_return_val_if_fail(COIL_IS_EXPANDABLE(self), NULL);
  g_return_val_if_fail(error == NULL || *error == NULL, NULL);
  g_return_val_if_fail(format, NULL);

  GString *buffer = g_string_sized_new(128);
  coil_expandable_build_string(self, buffer, format, error);

  return g_string_free(buffer, FALSE);
}

COIL_API(gboolean)
coil_expandable_equals(gconstpointer  e1,
                       gconstpointer  e2,
                       GError       **error) /* no need */
{
  g_return_val_if_fail(COIL_IS_EXPANDABLE(e1), FALSE);
  g_return_val_if_fail(COIL_IS_EXPANDABLE(e2), FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  if (e1 == e2)
    return TRUE;

  if (G_OBJECT_TYPE(e1) != G_OBJECT_TYPE(e2))
    return FALSE;

  CoilExpandable      *x1, *x2;
  CoilExpandableClass *klass;

  x1 = COIL_EXPANDABLE(e1);
  x2 = COIL_EXPANDABLE(e2);
  klass = COIL_EXPANDABLE_GET_CLASS(x1);

  return klass->equals(x1, x2, error);
}

COIL_API(gboolean)
coil_expandable_value_equals(const GValue  *v1,
                             const GValue  *v2,
                             GError       **error) /* no need */
{
  g_return_val_if_fail(G_IS_VALUE(v1), FALSE);
  g_return_val_if_fail(G_IS_VALUE(v2), FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  const CoilExpandable *x1, *x2;

  if (!(G_VALUE_HOLDS(v1, COIL_TYPE_EXPANDABLE)
    && G_VALUE_HOLDS(v2, COIL_TYPE_EXPANDABLE)))
    return FALSE;

  x1 = COIL_EXPANDABLE(g_value_get_object(v1));
  x2 = COIL_EXPANDABLE(g_value_get_object(v2));

  return coil_expandable_equals(x1, x2, error);
}

COIL_API(gboolean)
coil_is_expanded(CoilExpandable *self) /* const expandable pointer */
{
  g_return_val_if_fail(COIL_IS_EXPANDABLE(self), FALSE);

  CoilExpandableClass *klass = COIL_EXPANDABLE_GET_CLASS(self);
  return klass->is_expanded(self);
}

COIL_API(gboolean)
coil_expand(gpointer        object,
            const GValue  **value_ptr,
            gboolean        recursive,
            GError        **error)
{
  g_return_val_if_fail(COIL_IS_EXPANDABLE(object), FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  CoilExpandable        *self = COIL_EXPANDABLE(object);
  CoilExpandablePrivate *const priv = self->priv;
  CoilExpandableClass   *klass = COIL_EXPANDABLE_GET_CLASS(self);
  const GValue          *return_value = NULL;
  GError                *internal_error = NULL;

  /* TODO(jcon): notify container of expansion */

  if (!g_static_mutex_trylock(&priv->expand_lock))
  {
    /* TODO(jcon): improve error handling for cases like this */
    coil_struct_error(&internal_error,
                      COIL_IS_STRUCT(self) ? COIL_STRUCT(self) : self->container,
                      "Cycle detected during expansion");

    goto error;
  }

  if (!klass->expand(self, &return_value, error))
    goto error;

  if (recursive && return_value /* want to expand return value */
    && (value_ptr == NULL /* caller doesnt care about return value */
      || return_value != *value_ptr) /* prevent expand cycle on same value */
    && G_VALUE_HOLDS(return_value, COIL_TYPE_EXPANDABLE) /* must be expandable */
    && !coil_expand_value(return_value, &return_value, TRUE, error))
    goto error;

  g_static_mutex_unlock(&priv->expand_lock);

  if (value_ptr && return_value)
    *value_ptr = return_value;

  return TRUE;

error:
  if (value_ptr)
    *value_ptr = NULL;

  if (internal_error)
    g_propagate_error(error, internal_error);

  g_static_mutex_unlock(&priv->expand_lock);
  return FALSE;
}

COIL_API(gboolean)
coil_expand_value(const GValue  *value,
                  const GValue **return_value,
                  gboolean       recursive,
                  GError       **error)
{
  g_return_val_if_fail(G_IS_VALUE(value), FALSE);
  g_return_val_if_fail(G_VALUE_HOLDS(value, COIL_TYPE_EXPANDABLE), FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  CoilExpandable *object = COIL_EXPANDABLE(g_value_get_object(value));

  return coil_expand(object, return_value, recursive, error);
}

COIL_API(CoilExpandable *)
coil_expandable_copy(gconstpointer     object,
                     GError          **error,
                     const gchar      *first_property_name,
                     ...)
{
  g_return_val_if_fail(COIL_IS_EXPANDABLE(object), NULL);
  g_return_val_if_fail(error == NULL || *error == NULL, NULL);

  va_list properties;

  CoilExpandable      *exp = COIL_EXPANDABLE(object);
  CoilExpandableClass *klass = COIL_EXPANDABLE_GET_CLASS(exp);
  CoilExpandable      *result;

  va_start(properties, first_property_name);
  result = klass->copy(exp, first_property_name, properties, error);
  va_end(properties);

  return result;
}

static void
coil_expandable_set_property(GObject      *object,
                             guint         property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  CoilExpandable *self = COIL_EXPANDABLE(object);

  switch (property_id)
  {
    case PROP_CONTAINER:
    {
      self->container = g_value_get_object(value);
      break;
    }

    /* TODO(jcon): refactor */
    case PROP_LOCATION:
    {
      if (self->location.filepath)
        g_free(self->location.filepath);

      CoilLocation *loc_ptr;
      loc_ptr = (CoilLocation *)g_value_get_pointer(value);
      if (loc_ptr)
      {
        self->location = *((CoilLocation *)loc_ptr);
        self->location.filepath = g_strdup(loc_ptr->filepath);
      }
      break;
    }

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
      break;
  }
}

static void
coil_expandable_get_property(GObject    *object,
                             guint       property_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  CoilExpandable *self = COIL_EXPANDABLE(object);

  switch (property_id)
  {
    case PROP_CONTAINER:
      g_value_set_object(value, self->container);
      break;

    /* TODO(jcon): refactor */
    case PROP_LOCATION:
      g_value_set_pointer(value, &(self->location));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
      break;
  }
}

static void
coil_expandable_init(CoilExpandable *self)
{
  g_return_if_fail(COIL_IS_EXPANDABLE(self));

  CoilExpandablePrivate *priv = COIL_EXPANDABLE_GET_PRIVATE(self);
  self->priv = priv;

  g_static_mutex_init(&priv->expand_lock);
}

static CoilExpandable *
_expandable_copy(gconstpointer      self,
                 const gchar       *first_property_name,
                 va_list            properties,
                 GError           **error)
{
  g_error("Bad implementation of expandable->copy() in '%s' class.",
          G_OBJECT_CLASS_NAME(self));

  return NULL;
}

static gboolean
_expandable_is_expanded(gconstpointer self)
{
  g_error("Bad implementation of expandable->is_expanded() in '%s' class.",
          G_OBJECT_CLASS_NAME(self));

  return FALSE;
}

static gboolean
_expandable_expand(gconstpointer  self,
                   const GValue **return_value,
                   GError       **error)
{
  g_error("Bad implementation of expandable->expand() in '%s' class.",
          G_OBJECT_CLASS_NAME(self));

  return FALSE;
}

static gint
_expandable_equals(gconstpointer self,
                   gconstpointer other,
                   GError     **error)
{
  g_error("Bad implementation of expandable->equals() in '%s' class.",
          G_OBJECT_CLASS_NAME(self));

  return 0;
}

static void
_expandable_build_string(gconstpointer     self,
                         GString          *buffer,
                         CoilStringFormat *format,
                         GError          **error)
{
  g_error("Bad implementation of expandable->build_string() in '%s' class.",
          G_OBJECT_CLASS_NAME(self));
}

static void
coil_expandable_finalize(GObject *object)
{
  CoilExpandable        *const self = COIL_EXPANDABLE(object);
 /* CoilExpandablePrivate *const priv = self->priv; */

  /* TODO(jcon): refactor */
  g_free(self->location.filepath);
}

static void
coil_expandable_class_init(CoilExpandableClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

  g_type_class_add_private(gobject_class, sizeof(CoilExpandablePrivate));

  gobject_class->set_property = coil_expandable_set_property;
  gobject_class->get_property = coil_expandable_get_property;
  gobject_class->finalize = coil_expandable_finalize;

  /*
   * XXX: Override virtuals in sub-classes
   */

  klass->copy         = _expandable_copy;
  klass->is_expanded  = _expandable_is_expanded;
  klass->expand       = _expandable_expand;
  klass->equals       = _expandable_equals;
  klass->build_string = _expandable_build_string;

  /*
   * Properties
   */

  g_object_class_install_property(gobject_class, PROP_CONTAINER,
      g_param_spec_object("container",
                          "The container of this struct.",
                          "set/get the container of this struct.",
                          COIL_TYPE_STRUCT,
                          G_PARAM_CONSTRUCT |
                          G_PARAM_READWRITE));

  g_object_class_install_property(gobject_class, PROP_LOCATION,
      g_param_spec_pointer("location",
                         "Line, column, file of this instance.",
                         "get/set the location.",
                         G_PARAM_READWRITE));

}

