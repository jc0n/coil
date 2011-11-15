/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */
#ifndef __COIL_OBJECT_H
#define __COIL_OBJECT_H

#include "path.h"

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

#define coil_object_ref(obj) g_object_ref(G_OBJECT(obj))
#define coil_object_unref(obj) g_object_unref(G_OBJECT(obj))

typedef struct _CoilObject         CoilObject;
typedef struct _CoilObjectClass    CoilObjectClass;
typedef struct _CoilObjectPrivate  CoilObjectPrivate;

#include "value.h"

struct _CoilObject
{
    GObject             parent_instance;
    CoilObjectPrivate  *priv;

    /* * public * */
    CoilObject   *root;
    CoilObject   *container;
    CoilPath     *path;
    CoilLocation  location;
};

struct _CoilObjectClass
{
    GObjectClass parent_class;

    void (*set_path) (CoilObject *self, CoilPath *path);
    void (*set_container) (CoilObject *self, CoilObject *container);

    /* Abstract Methods */
    CoilObject *(*copy) (CoilObject *self, const gchar *first_property_name,
            va_list properties);

    gboolean (*is_expanded) (CoilObject *self);

    gboolean (*expand) (CoilObject *self, const GValue **return_value);

    gint (*equals) (CoilObject *self, CoilObject *other);

    void  (*build_string) (CoilObject *self, GString *buffer,
            CoilStringFormat *format);
};


G_BEGIN_DECLS

GType
coil_object_get_type(void) G_GNUC_CONST;

void
coil_object_get(CoilObject *object, const char *first_property_name, ...)
    G_GNUC_NULL_TERMINATED;

void
coil_object_set(CoilObject *object, const char *first_property_name, ...)
    G_GNUC_NULL_TERMINATED;

void
coil_object_set_container(CoilObject *object, CoilObject *container);

void
coil_object_set_path(CoilObject *object, CoilPath *path);

guint
coil_object_get_refcount(CoilObject *object);

CoilObject *
coil_object_copy(CoilObject *object, const gchar *first_property_name, ...)
    G_GNUC_NULL_TERMINATED;

void
coil_object_build_string(CoilObject *self, GString *buffer,
        CoilStringFormat *format);

gchar *
coil_object_to_string(CoilObject *self, CoilStringFormat *format);

gboolean
coil_object_equals(CoilObject *a, CoilObject *b);

 /* TODO(jcon): consider removing */
gboolean
coil_is_expanded(CoilObject *self);

 /* const object pointer */
gboolean
coil_expand_value(const GValue *value, const GValue **return_value,
        gboolean recursive);

/* replaced with coil_object_expand */
gboolean
coil_expand(CoilObject *object, const GValue **return_value,
        gboolean recursive);


/* TODO(jcon)
CoilObject *
coil_object_expand(CoilObject *object, gboolean recurse);
*/
#define coil_object_expand coil_expand
/*
gboolean
coil_object_expand(CoilObject *object,
                   const GValue **return_value,
                   gboolean recursive);
*/


G_END_DECLS

#endif /* COIL_OBJECT_H */
