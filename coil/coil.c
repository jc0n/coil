/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */
#include "common.h"
#include "struct.h"

/* TODO(jcon): namespace */
CoilStringFormat default_string_format = {
    ( LEGACY
      | BLANK_LINE_AFTER_BRACE
      | BLANK_LINE_AFTER_STRUCT
      | ESCAPE_QUOTES
    ),
    4, /* block indent */
    0,  /* brace indent */
    78, /* multiline len */
    0, /* indent level */
    (CoilObject *)NULL, /* context */
};

/*
 * coil_init:
 *
 * Call this before using coil. Initializes the type system
 * and the coil none type.
 */
void
coil_init(void)
{
    static gboolean init_called = FALSE;
    g_assert(init_called == FALSE);

    g_type_init();
    //  g_type_init_with_debug_flags(G_TYPE_DEBUG_SIGNALS);

    coil_none_object = g_object_new(COIL_TYPE_NONE, NULL);
    coil_path_ref(CoilRootPath);
    init_called = TRUE;
}

/*
 * coil_location_get_type:
 *
 * Get the type identifier for #CoilLocation
 */
GType
coil_location_get_type(void)
{
    static GType type_id = 0;

    if (!type_id)
        type_id = g_pointer_type_register_static("CoilLocation");

    return type_id;
}

static void
set_pointer_from_value(gpointer ptr, const GValue *value)
{
    g_return_if_fail(pointer);

    GType value_type = G_VALUE_TYPE(value);

    switch (G_TYPE_FUNDAMENTAL(value_type)) {
        case COIL_TYPE_BOOLEAN:
            *(gboolean *)ptr = g_value_get_boolean(value);
            break;
        case COIL_TYPE_INT:
            *(gint *)ptr = g_value_get_int(value);
            break;
        case COIL_TYPE_UINT:
            *(guint *)ptr = g_value_get_uint(value);
            break;
        case COIL_TYPE_LONG:
            *(glong *)ptr = g_value_get_long(value);
            break;
        case COIL_TYPE_ULONG:
            *(gulong *)ptr = g_value_get_ulong(value);
            break;
        case COIL_TYPE_INT64:
            *(gint64 *)ptr = g_value_get_int64(value);
            break;
        case COIL_TYPE_UINT64:
            *(guint64 *)ptr = g_value_get_uint64(value);
            break;
        case COIL_TYPE_FLOAT:
            *(gfloat *)ptr = g_value_get_float(value);
            break;
        case COIL_TYPE_DOUBLE:
            *(gdouble *)ptr = g_value_get_double(value);
            break;
        case COIL_TYPE_STRING:
            *(gpointer *)ptr = (gpointer)g_value_dup_string(value);
            break;
        case G_TYPE_OBJECT:
            if (value_type == COIL_TYPE_NONE) {
                ptr = NULL;
                break;
            }
            else if (value_type == COIL_TYPE_OBJECT) {
                ptr = (gpointer)g_value_dup_object(value);
                break;
            }
        default:
            g_error("Unsupported coil value type");
    }
}

static void
set_value_from_pointer(GValue *value, gconstpointer pointer) __attribute__((unused));

static void
set_value_from_pointer(GValue *value, gconstpointer pointer)
{
    GType value_type = G_VALUE_TYPE(value);

    switch (G_TYPE_FUNDAMENTAL(value_type)) {
        case COIL_TYPE_BOOLEAN:
            g_value_set_boolean(value, *(gboolean *)pointer);
            break;
        case COIL_TYPE_INT:
            g_value_set_int(value, *(gint *)pointer);
            break;
        case COIL_TYPE_UINT:
            g_value_set_uint(value, *(guint *)pointer);
            break;
        case COIL_TYPE_LONG:
            g_value_set_long(value, *(glong *)pointer);
            break;
        case COIL_TYPE_ULONG:
            g_value_set_ulong(value, *(gulong *)pointer);
            break;
        case COIL_TYPE_INT64:
            g_value_set_int64(value, *(gint64 *)pointer);
            break;
        case COIL_TYPE_UINT64:
            g_value_set_uint64(value, *(guint64 *)pointer);
            break;
        case COIL_TYPE_FLOAT:
            g_value_set_float(value, *(gfloat *)pointer);
            break;
        case COIL_TYPE_DOUBLE:
            g_value_set_double(value, *(gdouble *)pointer);
            break;
        case COIL_TYPE_STRING:
            g_value_set_string(value, (gchar *)pointer);
            break;
        case G_TYPE_OBJECT:
            if (value_type == COIL_TYPE_NONE) {
                g_value_set_object(value, coil_none_object);
                break;
            }
            else if (g_type_is_a(value_type, COIL_TYPE_OBJECT)) {
                g_value_set_object(value, (CoilObject *)pointer);
                break;
            }
        default:
            g_error("Unsupported coil value type");
    }
}

const char *
coil_type_name(GType type)
{
    switch (G_TYPE_FUNDAMENTAL(type)) {
        case COIL_TYPE_BOOLEAN:
            return "boolean";
        case COIL_TYPE_INT:
            return "int";
        case COIL_TYPE_UINT:
            return "uint";
        case COIL_TYPE_LONG:
            return "long";
        case COIL_TYPE_ULONG:
            return "ulong";
        case COIL_TYPE_INT64:
            return "int64";
        case COIL_TYPE_UINT64:
            return "uint64";
        case COIL_TYPE_FLOAT:
            return "float";
        case COIL_TYPE_DOUBLE:
            return "double";
        case COIL_TYPE_STRING:
            return "string";
        case G_TYPE_OBJECT:
            if (type == COIL_TYPE_NONE)
                return "none";
            else if (type == COIL_TYPE_OBJECT)
                return "object";
        default:
            return g_type_name(type);
    }
}

static gboolean
get_and_transform_type(CoilObject *o, const char *key,
        int return_type, gpointer ptr)
{
    g_return_val_if_fail(vptr != NULL, FALSE);

    CoilPath *path = NULL;
    GType value_type;
    const GValue *value;
    gboolean result = FALSE;

    path = coil_path_new(key);
    if (path == NULL) {
        goto end;
    }
    value = coil_struct_lookupx(o, path, TRUE);
    if (value == NULL) {
        goto end;
    }
    value_type = G_VALUE_TYPE(value);
    if (value_type == return_type) {
        set_pointer_from_value(ptr, value);
        result = TRUE;
    }
    else if (g_value_type_transformable(value_type, return_type)) {
        GValue tmp = {0,};

        g_value_init(&tmp, return_type);
        g_value_transform(value, &tmp);

        set_pointer_from_value(ptr, &tmp);
        g_value_unset(&tmp);
        result = TRUE;
    }
    else {
        coil_set_error(COIL_ERROR_VALUE_TYPE, NULL,
                "Unable to convert %s value type at '%s' to type %s.",
                coil_type_name(value_type), path->str,
                coil_type_name(return_type));
    }
end:
    if (result == FALSE) {
        ptr = NULL;
    }
    coil_path_unrefx(path);
    return result;
}

/*
 * coil_get:
 * coil_get(o, 'x', COIL_TYPE_INT, &intval)
 * coil_set(o, 'y', COIL_TYPE_INT, &intval)
 * coil_get(o, 'x.y', COIL_TYPE_INT, &intval)
 */
gboolean
coil_get(CoilObject *o, const char *key, int type, gpointer return_value)
{
    return get_and_transform_type(o, key, type, return_value);
}

/*
 * coil_getx:
 */

/*
 * coil_set:
 */

/*
 * coil_setx:
 */
