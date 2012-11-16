/*
 * Copyright (C) 2012 John O'Connor
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include "common.h"
#include "struct.h"
#include "link.h"

G_DEFINE_ABSTRACT_TYPE(CoilObject, coil_object, G_TYPE_OBJECT);

#define COIL_OBJECT_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE((obj), COIL_TYPE_OBJECT, \
                               CoilObjectPrivate))

struct _CoilObjectPrivate
{
    GMutex  expand_lock;
};

typedef enum
{
    PROP_O,
    PROP_CONTAINER,
    PROP_LOCATION,
    PROP_PATH,
    PROP_ROOT,
    PROP_LAST,
} CoilObjectProperties;

static GParamSpec *properties[PROP_LAST];


COIL_API(void)
coil_object_build_string(CoilObject *self, GString *buffer,
        CoilStringFormat *format)
{
    g_return_if_fail(COIL_IS_OBJECT(self));
    g_return_if_fail(format);

    CoilObjectClass *klass = COIL_OBJECT_GET_CLASS(self);
    return klass->build_string(self, buffer, format);
}

COIL_API(gchar *)
coil_object_to_string(CoilObject *self, CoilStringFormat *format)
{
    g_return_val_if_fail(COIL_IS_OBJECT(self), NULL);
    g_return_val_if_fail(format, NULL);

    GString *buffer = g_string_sized_new(128);
    coil_object_build_string(self, buffer, format);
    if (coil_error_occurred()) {
        g_string_free(buffer, TRUE);
        return NULL;
    }
    return g_string_free(buffer, FALSE);
}

static void
object_value_to_string_value(const GValue *objval, GValue *strval)
{
    CoilObject *obj;
    char *string;

    obj = g_value_dup_object(objval);
    if (obj == NULL) {
        return;
    }
    string = coil_object_to_string(obj, &default_string_format);
    g_value_take_string(strval, string);
}

COIL_API(gboolean)
coil_object_equals(CoilObject *a, CoilObject *b)
{
    g_return_val_if_fail(COIL_IS_OBJECT(a), FALSE);
    g_return_val_if_fail(COIL_IS_OBJECT(b), FALSE);

    CoilObjectClass *klass;

    if (a == b)
        return TRUE;

    if (G_OBJECT_TYPE(a) != G_OBJECT_TYPE(b))
        return FALSE;

    klass = COIL_OBJECT_GET_CLASS(a);
    return klass->equals(a, b);
}

COIL_API(gboolean)
coil_is_expanded(CoilObject *self)
{
    g_return_val_if_fail(COIL_IS_OBJECT(self), FALSE);

    CoilObjectClass *klass = COIL_OBJECT_GET_CLASS(self);
    return klass->is_expanded(self);
}

COIL_API(gboolean)
coil_expand(CoilObject *object, const GValue **value_ptr, gboolean recursive)
{
    g_return_val_if_fail(COIL_IS_OBJECT(object), FALSE);

    CoilObject *self = COIL_OBJECT(object);
    CoilObjectPrivate *priv = self->priv;
    CoilObjectClass *klass = COIL_OBJECT_GET_CLASS(self);
    const GValue *return_value = NULL;

    if (klass->expand == NULL) {
        /* *value_ptr = self_value */
        return TRUE;
    }

    /* TODO(jcon): notify container of expansion */

    if (!g_mutex_trylock(&priv->expand_lock)) {
        /* TODO(jcon): improve error handling for cases like this */
        CoilObject *container = COIL_IS_STRUCT(self) ? self : self->container;
        coil_struct_error(container, "Cycle detected during expansion");
        goto error;
    }

    if (!klass->expand(self, &return_value))
        goto error;

    if (recursive && return_value /* want to expand return value */
            && (value_ptr == NULL /* caller doesnt care about return value */
                || return_value != *value_ptr) /* prevent expand cycle on same value */
            && G_VALUE_HOLDS(return_value, COIL_TYPE_OBJECT) /* must be object */
            && !coil_expand_value(return_value, &return_value, TRUE))
        goto error;

    if (return_value == NULL && !COIL_IS_STRUCT(object)) {
        g_error("Expecting return value from expansion of type '%s'.",
                G_OBJECT_TYPE_NAME(object));
    }
    g_mutex_unlock(&priv->expand_lock);

    if (value_ptr && return_value)
        *value_ptr = return_value;

    return TRUE;

error:
    if (value_ptr) {
        *value_ptr = NULL;
    }
    g_mutex_unlock(&priv->expand_lock);
    return FALSE;
}

COIL_API(gboolean)
coil_expand_value(const GValue *value, const GValue **return_value,
        gboolean recursive)
{
    g_return_val_if_fail(G_IS_VALUE(value), FALSE);
    g_return_val_if_fail(G_VALUE_HOLDS(value, COIL_TYPE_OBJECT), FALSE);

    CoilObject *object = COIL_OBJECT(g_value_get_object(value));

    return coil_expand(object, return_value, recursive);
}

COIL_API(CoilObject *)
coil_value_get_object(const CoilValue *value)
{
    g_return_val_if_fail(value != NULL, NULL);
    g_return_val_if_fail(G_IS_VALUE(value), NULL);
    g_return_val_if_fail(G_VALUE_HOLDS(value, COIL_TYPE_OBJECT), NULL);

    return COIL_OBJECT(g_value_get_object(value));
}

COIL_API(CoilObject *)
coil_value_dup_object(const CoilValue *value)
{
    g_return_val_if_fail(value != NULL, NULL);
    g_return_val_if_fail(G_IS_VALUE(value), NULL);
    g_return_val_if_fail(G_VALUE_HOLDS(value, COIL_TYPE_OBJECT), NULL);

    return COIL_OBJECT(g_value_dup_object(value));
}

COIL_API(CoilObject *)
coil_object_copy(CoilObject *object, const gchar *first_property_name, ...)
{
    g_return_val_if_fail(COIL_IS_OBJECT(object), NULL);

    va_list properties;

    CoilObject *exp = COIL_OBJECT(object);
    CoilObjectClass *klass = COIL_OBJECT_GET_CLASS(exp);
    CoilObject *result;

    va_start(properties, first_property_name);
    result = klass->copy(exp, first_property_name, properties);
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
        case PROP_CONTAINER: {
            CoilObject *container = COIL_OBJECT(g_value_get_object(value));
            if (container) {
                g_object_freeze_notify(object);
                coil_object_set_container(COIL_OBJECT(object), container);
                g_object_thaw_notify(object);
            }
            break;
        }
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
        case PROP_PATH: {
            CoilPath *path = (CoilPath *)g_value_dup_boxed(value);
            if (path) {
                g_object_freeze_notify(object);
                coil_object_set_path(COIL_OBJECT(object), path);
                g_object_thaw_notify(object);
            }
            break;
        }
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

void
coil_object_set_container(CoilObject *object, CoilObject *container)
{
    g_return_if_fail(object);
    g_return_if_fail(COIL_IS_OBJECT(object));
    g_return_if_fail(container == NULL || COIL_IS_OBJECT(container));

    CoilObjectClass *klass = COIL_OBJECT_GET_CLASS(object);

    if (klass->set_container != NULL) {
        klass->set_container(object, container);
    }
    if (container != NULL) {
        object->container = container;
        object->root = object->container->root;
    }
    else {
        object->container = NULL;
        object->root = object;
    }

#if GLIB_MAJOR_VERSION >= 2 && GLIB_MINOR_VERSION >= 6
    g_object_notify_by_pspec(G_OBJECT(object), properties[PROP_CONTAINER]);
    g_object_notify_by_pspec(G_OBJECT(object), properties[PROP_ROOT]);
#else
    g_object_notify(G_OBJECT(object), properties[PROP_CONTAINER]->name);
    g_object_notify(G_OBJECT(object), properties[PROP_ROOT]->name);
#endif
}

void
coil_object_set_path(CoilObject *object, CoilPath *path)
{
    g_return_if_fail(object);
    g_return_if_fail(path);

    CoilObjectClass *klass = COIL_OBJECT_GET_CLASS(object);

    if (klass->set_path != NULL) {
        klass->set_path(object, path);
    }
    object->path = path;
#if GLIB_MAJOR_VERSION >= 2 && GLIB_MINOR_VERSION >= 6
    g_object_notify_by_pspec(G_OBJECT(object), properties[PROP_PATH]);
#else
    g_object_notify(G_OBJECT(object), properties[PROP_PATH]->name);
#endif
}

inline guint
coil_object_get_refcount(CoilObject *object)
{
    GObject *gobject = G_OBJECT(object);
    return g_atomic_int_get(&gobject->ref_count);
}

static void
coil_object_init(CoilObject *self)
{
    g_return_if_fail(COIL_IS_OBJECT(self));

    CoilObjectPrivate *priv = COIL_OBJECT_GET_PRIVATE(self);
    self->priv = priv;

    g_mutex_init(&priv->expand_lock);
}

static CoilObject *
object_copy(CoilObject *self, const gchar *first_property_name,
        va_list properties)
{
    g_error("Missing implementation of object->copy() in '%s' class.",
            G_OBJECT_CLASS_NAME(self));

    return NULL;
}

static gboolean
object_is_expanded(CoilObject *self)
{
    CoilObjectClass *klass = COIL_OBJECT_CLASS(self);
    if (klass->expand == NULL) {
        return TRUE;
    }
    g_error("Missing implementation of object->is_expanded() in '%s' class.",
            G_OBJECT_CLASS_NAME(self));

    return FALSE;
}

static gint
object_equals(CoilObject * self, CoilObject * other)
{
    g_error("Missing implementation of object->equals() in '%s' class.",
            G_OBJECT_CLASS_NAME(self));

    return 0;
}

static void
object_build_string(CoilObject *self, GString *buffer,
        CoilStringFormat *format)
{
    g_error("Missing implementation of object->build_string() in '%s' class.",
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

inline void
coil_object_unrefx(CoilObject *obj)
{
    if (obj != NULL) {
        coil_object_unref(obj);
    }
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
    klass->copy = object_copy;
    klass->is_expanded = object_is_expanded;
    klass->expand = NULL;
    klass->equals = object_equals;
    klass->build_string = object_build_string;

    /*
     * Properties
     */
    properties[PROP_CONTAINER] = g_param_spec_object("container",
            "The container of the object.",
            "set/get the container of this object.",
            COIL_TYPE_STRUCT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_ROOT] = g_param_spec_object("root",
            "The root of this object.",
            "set/get the container of this object.",
            COIL_TYPE_STRUCT, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_PATH] = g_param_spec_boxed("path",
            "The path of the object",
            "set/get the path of this object",
            COIL_TYPE_PATH, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_LOCATION] = g_param_spec_pointer("location",
            "Line, column, file of this instance.",
            "get/set the location.",
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    g_object_class_install_property(gobject_class, PROP_CONTAINER,
            properties[PROP_CONTAINER]);

    g_object_class_install_property(gobject_class, PROP_ROOT,
            properties[PROP_ROOT]);

    g_object_class_install_property(gobject_class, PROP_LOCATION,
            properties[PROP_LOCATION]);

    g_object_class_install_property(gobject_class, PROP_PATH,
            properties[PROP_PATH]);


    g_value_register_transform_func(COIL_TYPE_OBJECT, COIL_TYPE_STRING,
            object_value_to_string_value);
}

