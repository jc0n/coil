/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */
#ifndef __COIL_EXPANDABLE_H
#define __COIL_EXPANDABLE_H

#include "error.h"
#include "path.h"
#include "value.h"

#define COIL_TYPE_EXPANDABLE          \
        (coil_expandable_get_type())

#define COIL_EXPANDABLE(obj)          \
        (G_TYPE_CHECK_INSTANCE_CAST((obj), COIL_TYPE_EXPANDABLE, \
          CoilExpandable))

#define COIL_IS_EXPANDABLE(obj)       \
        (G_TYPE_CHECK_INSTANCE_TYPE((obj), COIL_TYPE_EXPANDABLE))

#define COIL_EXPANDABLE_CLASS(klass)  \
        (G_TYPE_CHECK_CLASS_CAST((klass), COIL_TYPE_EXPANDABLE, \
          CoilExpandableClass))

#define COIL_IS_EXPANDABLE_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_TYPE((klass), COIL_TYPE_EXPANDABLE))

#define COIL_EXPANDABLE_GET_CLASS(obj)  \
        (G_TYPE_INSTANCE_GET_CLASS((obj), COIL_TYPE_EXPANDABLE, \
          CoilExpandableClass))

#define coil_expandable_error_new(code, exp, format, args...)         \
        coil_error_new(code, (COIL_EXPANDABLE(exp))->location,        \
                        format, ## args)

#define coil_expandable_error_new_literal(code, exp, message)         \
        coil_error_new_literal(code,                                  \
                                (COIL_EXPANDABLE(exp))->location,     \
                                message)

typedef struct _CoilExpandable         CoilExpandable;
typedef struct _CoilExpandableClass    CoilExpandableClass;
typedef struct _CoilExpandablePrivate  CoilExpandablePrivate;

struct _CoilExpandable
{
  GObject                parent_instance;
  CoilExpandablePrivate *priv;

  /* * public * */
  CoilStruct   *root;
  CoilStruct   *container;
  CoilLocation  location;
};

struct _CoilExpandableClass
{
  GObjectClass parent_class;

  /* Abstract Methods */
  CoilExpandable *(*copy) (gconstpointer     self,
                           const CoilStruct *container,
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
coil_expandable_get_type(void) G_GNUC_CONST;

CoilExpandable *
coil_expandable_copy(gconstpointer     object,
                     const CoilStruct *container,
                     GError          **error);

void
coil_expandable_build_string(CoilExpandable   *self,
                             GString *const    buffer,
                             CoilStringFormat *format,
                             GError          **error);

gchar *
coil_expandable_to_string(CoilExpandable   *self,
                          CoilStringFormat *format,
                          GError          **error);

gboolean
coil_expandable_equals(gconstpointer  e1,
                       gconstpointer  e2,
                       GError       **error);

 /* TODO(jcon): consider removing */
gboolean
coil_expandable_value_equals(const GValue  *v1,
                             const GValue  *v2,
                             GError       **error);

 /* TODO(jcon): consider removing */
gboolean
coil_is_expanded(CoilExpandable *self);

 /* const expandable pointer */
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

#endif /* COIL_EXPANDABLE_H */
