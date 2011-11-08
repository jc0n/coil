/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */
#ifndef __COIL_OBJECT_H
#define __COIL_OBJECT_H

#include "path.h"
#include "value.h"

#define COIL_TYPE_OBJECT          \
        (coil_object_get_type())

#define COIL_OBJECT(obj)          \
        (G_TYPE_CHECK_INSTANCE_CAST((obj), COIL_TYPE_OBJECT, \
          CoilObject))

#define COIL_IS_OBJECT(obj)       \
        (G_TYPE_CHECK_INSTANCE_TYPE((obj), COIL_TYPE_OBJECT))

#define COIL_OBJECT_CLASS(klass)  \
        (G_TYPE_CHECK_CLASS_CAST((klass), COIL_TYPE_OBJECT, \
          CoilObjectClass))

#define COIL_IS_OBJECT_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_TYPE((klass), COIL_TYPE_OBJECT))

#define COIL_OBJECT_GET_CLASS(obj)  \
        (G_TYPE_INSTANCE_GET_CLASS((obj), COIL_TYPE_OBJECT, \
          CoilObjectClass))

typedef struct _CoilObject         CoilObject;
typedef struct _CoilObjectClass    CoilObjectClass;
typedef struct _CoilObjectPrivate  CoilObjectPrivate;

struct _CoilObject
{
  GObject             parent_instance;
  CoilObjectPrivate  *priv;

  /* * public * */
  CoilStruct   *root;
  CoilStruct   *container;
  CoilLocation  location;
};

struct _CoilObjectClass
{
  GObjectClass parent_class;

  /* Abstract Methods */
  CoilObject *(*copy) (gconstpointer     self,
                           const gchar      *first_property_name,
                           va_list           properties,
                           GError          **error);

  gboolean (*is_expanded) (gconstpointer self);

  gboolean (*expand) (gconstpointer   self,
                      const GValue  **return_value,
                      GError        **error);

  gint (*equals) (gconstpointer  self,
                  gconstpointer  other,
                  GError        **error);

  void  (*build_string) (gconstpointer     self,
                         GString          *buffer,
                         CoilStringFormat *format,
                         GError          **error);
};

G_BEGIN_DECLS

GType
coil_object_get_type(void) G_GNUC_CONST;

CoilObject *
coil_object_copy(gconstpointer     object,
                     GError          **error,
                     const gchar      *first_property_name,
                     ...) G_GNUC_NULL_TERMINATED;

void
coil_object_build_string(CoilObject   *self,
                             GString *const    buffer,
                             CoilStringFormat *format,
                             GError          **error);

gchar *
coil_object_to_string(CoilObject   *self,
                          CoilStringFormat *format,
                          GError          **error);

gboolean
coil_object_equals(gconstpointer  e1,
                       gconstpointer  e2,
                       GError       **error);

 /* TODO(jcon): consider removing */
gboolean
coil_object_value_equals(const GValue  *v1,
                             const GValue  *v2,
                             GError       **error);

 /* TODO(jcon): consider removing */
gboolean
coil_is_expanded(CoilObject *self);

 /* const object pointer */
gboolean
coil_expand_value(const GValue  *value,
                  const GValue **return_value,
                  gboolean       recursive,
                  GError       **error);

gboolean
coil_expand(gpointer        object,
            const GValue  **return_value,
            gboolean        recursive,
            GError        **error);

G_END_DECLS

#endif /* COIL_OBJECT_H */
