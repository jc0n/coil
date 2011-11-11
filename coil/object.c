/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */

#include <string.h>

#include "common.h"
#include "struct.h"
#include "link.h"

G_DEFINE_ABSTRACT_TYPE(CoilObject, coil_object, G_TYPE_OBJECT);

#define COIL_OBJECT_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE((obj), COIL_TYPE_OBJECT, \
                               CoilObjectPrivate))

struct _CoilObjectPrivate
{
    GStaticMutex  expand_lock;
};

typedef enum
{
    PROP_O,
    PROP_CONTAINER,
    PROP_LOCATION,
    PROP_PATH,
    PROP_ROOT,
} CoilObjectProperties;

COIL_API(void)
coil_object_build_string(CoilObject   *self,
                             GString          *const buffer,
                             CoilStringFormat *format,
                             GError          **error)
{
    g_return_if_fail(COIL_IS_OBJECT(self));
    g_return_if_fail(error == NULL || *error == NULL);
    g_return_if_fail(format);

    CoilObjectClass *klass = COIL_OBJECT_GET_CLASS(self);
    return klass->build_string(self, buffer, format, error);
}

COIL_API(gchar *)
coil_object_to_string(CoilObject   *self,
        CoilStringFormat *format,
        GError          **error)
{
    g_return_val_if_fail(COIL_IS_OBJECT(self), NULL);
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);
    g_return_val_if_fail(format, NULL);

    GString *buffer = g_string_sized_new(128);
    coil_object_build_string(self, buffer, format, error);

    return g_string_free(buffer, FALSE);
}

COIL_API(gboolean)
coil_object_equals(gconstpointer  e1,
                       gconstpointer  e2,
                       GError       **error) /* no need */
{
    g_return_val_if_fail(COIL_IS_OBJECT(e1), FALSE);
    g_return_val_if_fail(COIL_IS_OBJECT(e2), FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    if (e1 == e2)
        return TRUE;

    if (G_OBJECT_TYPE(e1) != G_OBJECT_TYPE(e2))
        return FALSE;

    CoilObject      *x1, *x2;
    CoilObjectClass *klass;

    x1 = COIL_OBJECT(e1);
    x2 = COIL_OBJECT(e2);
    klass = COIL_OBJECT_GET_CLASS(x1);

    return klass->equals(x1, x2, error);
}

COIL_API(gboolean)
coil_object_value_equals(const GValue  *v1,
                             const GValue  *v2,
                             GError       **error) /* no need */
{
    g_return_val_if_fail(G_IS_VALUE(v1), FALSE);
    g_return_val_if_fail(G_IS_VALUE(v2), FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    const CoilObject *x1, *x2;

    if (!(G_VALUE_HOLDS(v1, COIL_TYPE_OBJECT)
                && G_VALUE_HOLDS(v2, COIL_TYPE_OBJECT)))
        return FALSE;

    x1 = COIL_OBJECT(g_value_get_object(v1));
    x2 = COIL_OBJECT(g_value_get_object(v2));

    return coil_object_equals(x1, x2, error);
}

COIL_API(gboolean)
coil_is_expanded(CoilObject *self) /* const object pointer */
{
    g_return_val_if_fail(COIL_IS_OBJECT(self), FALSE);

    CoilObjectClass *klass = COIL_OBJECT_GET_CLASS(self);
    return klass->is_expanded(self);
}

COIL_API(gboolean)
coil_expand(CoilObject *object, const GValue **value_ptr,
        gboolean recursive, GError **error)
{
    g_return_val_if_fail(COIL_IS_OBJECT(object), FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    CoilObject *self = COIL_OBJECT(object);
    CoilObjectPrivate *priv = self->priv;
    CoilObjectClass *klass = COIL_OBJECT_GET_CLASS(self);
    const GValue *return_value = NULL;
    GError *internal_error = NULL;

    /* TODO(jcon): notify container of expansion */

    if (!g_static_mutex_trylock(&priv->expand_lock)) {
        /* TODO(jcon): improve error handling for cases like this */
        CoilObject *container;
        if (COIL_IS_STRUCT(self)) {
            container = self;
        }
        else {
            container = self->container;
        }
        coil_struct_error(&internal_error, container,
                "Cycle detected during expansion");
        goto error;
    }

    if (!klass->expand(self, &return_value, error))
        goto error;

    if (recursive && return_value /* want to expand return value */
            && (value_ptr == NULL /* caller doesnt care about return value */
                || return_value != *value_ptr) /* prevent expand cycle on same value */
            && G_VALUE_HOLDS(return_value, COIL_TYPE_OBJECT) /* must be object */
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
    g_return_val_if_fail(G_VALUE_HOLDS(value, COIL_TYPE_OBJECT), FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    CoilObject *object = COIL_OBJECT(g_value_get_object(value));

    return coil_expand(object, return_value, recursive, error);
}

COIL_API(CoilObject *)
coil_object_copy(gconstpointer     object,
                     GError          **error,
                     const gchar      *first_property_name,
                     ...)
{
    g_return_val_if_fail(COIL_IS_OBJECT(object), NULL);
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    va_list properties;

    CoilObject      *exp = COIL_OBJECT(object);
    CoilObjectClass *klass = COIL_OBJECT_GET_CLASS(exp);
    CoilObject      *result;

    va_start(properties, first_property_name);
    result = klass->copy(exp, first_property_name, properties, error);
    va_end(properties);

    return result;
}

static void
coil_object_set_property(GObject *object,
                         guint property_id,
                         const GValue *value,
                         GParamSpec *pspec)
{
    CoilObject *self = COIL_OBJECT(object);

    switch (property_id) {
        case PROP_CONTAINER:
            self->container = COIL_OBJECT(g_value_get_object(value));
            if (self->container) {
                self->root = self->container->root;
            }
            break;
        case PROP_LOCATION: {
            CoilLocation *loc = (CoilLocation *)g_value_get_pointer(value);
            if (self->location.filepath) {
                g_free(self->location.filepath);
            }
            if (loc) {
                self->location = *((CoilLocation *)loc);
                if (loc->filepath) {
                    self->location.filepath = g_strdup(loc->filepath);
                }
            }
            break;
        }
        case PROP_PATH:
            self->path = (CoilPath *)g_value_dup_boxed(value);
            break;
        case PROP_ROOT:
            self->root = COIL_OBJECT(g_value_get_object(value));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
coil_object_get_property(GObject *object,
                         guint property_id,
                         GValue *value,
                         GParamSpec *pspec)
{
    CoilObject *self = COIL_OBJECT(object);

    switch (property_id) {
        case PROP_CONTAINER:
            g_value_set_object(value, self->container);
            break;
            /* TODO(jcon): refactor */
        case PROP_LOCATION:
            g_value_set_pointer(value, &(self->location));
            break;
        case PROP_ROOT:
            g_value_set_object(value, self->root);
            break;
        case PROP_PATH:
            g_value_set_boxed(value, self->path);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

void
coil_object_get(CoilObject *object, const char *first_property_name, ...)
{
    g_return_if_fail(COIL_IS_OBJECT(object));

    va_list args;

    va_start(args, first_property_name);
    g_object_get_valist(G_OBJECT(object), first_property_name, args);
    va_end(args);
}

void
coil_object_set(CoilObject *object, const char *first_property_name, ...)
{
    g_return_if_fail(COIL_IS_OBJECT(object));

    va_list args;

    va_start(args, first_property_name);
    g_object_set_valist(G_OBJECT(object), first_property_name, args);
    va_end(args);
}

inline guint
coil_object_get_refcount(CoilObject *object)
{
    return G_OBJECT(object)->ref_count;
}

static void
coil_object_init(CoilObject *self)
{
    g_return_if_fail(COIL_IS_OBJECT(self));

    CoilObjectPrivate *priv = COIL_OBJECT_GET_PRIVATE(self);
    self->priv = priv;

    g_static_mutex_init(&priv->expand_lock);
}

static CoilObject *
_object_copy(CoilObject * self,
             const gchar *first_property_name,
             va_list properties,
             GError **error)
{
    g_error("Bad implementation of object->copy() in '%s' class.",
            G_OBJECT_CLASS_NAME(self));

    return NULL;
}

static gboolean
_object_is_expanded(CoilObject * self)
{
    g_error("Bad implementation of object->is_expanded() in '%s' class.",
            G_OBJECT_CLASS_NAME(self));

    return FALSE;
}

static gboolean
_object_expand(CoilObject * self, const GValue **return_value, GError **error)
{
    g_error("Bad implementation of object->expand() in '%s' class.",
            G_OBJECT_CLASS_NAME(self));

    return FALSE;
}

static gint
_object_equals(CoilObject * self, CoilObject * other, GError **error)
{
    g_error("Bad implementation of object->equals() in '%s' class.",
            G_OBJECT_CLASS_NAME(self));

    return 0;
}

static void
_object_build_string(CoilObject * self,
                     GString *buffer,
                     CoilStringFormat *format,
                     GError **error)
{
    g_error("Bad implementation of object->build_string() in '%s' class.",
            G_OBJECT_CLASS_NAME(self));
}

static void
coil_object_finalize(GObject *object)
{
    CoilObject *const self = COIL_OBJECT(object);
    /* CoilObjectPrivate *const priv = self->priv; */

    /* TODO(jcon): refactor */
    g_free(self->location.filepath);
}

static void
coil_object_class_init(CoilObjectClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(gobject_class, sizeof(CoilObjectPrivate));

    gobject_class->set_property = coil_object_set_property;
    gobject_class->get_property = coil_object_get_property;
    gobject_class->finalize = coil_object_finalize;

    /*
     * XXX: Override virtuals in sub-classes
     */
    klass->copy = _object_copy;
    klass->is_expanded = _object_is_expanded;
    klass->expand = _object_expand;
    klass->equals = _object_equals;
    klass->build_string = _object_build_string;
    /*
     * Properties
     */
    g_object_class_install_property(gobject_class, PROP_CONTAINER,
            g_param_spec_object("container",
                "The container of the object.",
                "set/get the container of this object.",
                COIL_TYPE_STRUCT,
                G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_ROOT,
            g_param_spec_object("root",
                "The root of this object.",
                "set/get the container of this object.",
                COIL_TYPE_STRUCT,
                G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_LOCATION,
            g_param_spec_pointer("location",
                "Line, column, file of this instance.",
                "get/set the location.",
                G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_PATH,
            g_param_spec_boxed("path",
                "The path of the object",
                "set/get the path of this object",
                COIL_TYPE_PATH,
                G_PARAM_CONSTRUCT | G_PARAM_READWRITE));
}

