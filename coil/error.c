/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */
#include "coil.h"


/* XXX: implementation is not threadsafe now for a number of reasons
 *
 * - When multithreading support is added errors should be separated for each
 *   thread.
 */
static GError *_coil_error = NULL;

/*
 * coil_error_quark:
 *
 * Return error identifier for Glib Error Handling
 */
GQuark
coil_error_quark(void)
{
    static GQuark result = 0;

    if (!result) {
        result = g_quark_from_static_string("coil-error-quark");
    }
    return result;
}

inline static char *
get_location_prefix(CoilLocation *location)
{
    if (location) {
        int line = location->line;
        const char *filepath = location->filepath;

        if (line > 0 && filepath != NULL)
            return g_strdup_printf("line %d in file %s: ", line, filepath);
        else if (line > 0)
            return g_strdup_printf("line %d: ", line);
        else if (filepath != NULL)
            return g_strdup_printf("file %s: ", filepath);
    }
    return g_strndup("", 0);
}

inline static char *
get_short_path_string(const gchar *str, guint len)
{
    if (str == NULL || len == 0) {
        return g_strndup("Unknown Path", 12);
    }
    return strtrunc("<...>", TRUNCATE_CENTER, 40, str, len);
}

inline static char *
get_short_path(CoilPath *path)
{
    if (path == NULL) {
        return g_strndup("", 0);
    }
    return get_short_path_string(path->str, path->len);
}

gboolean
coil_error_occurred(void)
{
    return _coil_error != NULL;
}

gboolean
coil_get_error(GError **error)
{
    if (coil_error_occurred()) {
        g_propagate_error(error, _coil_error);
        return TRUE;
    }
    return FALSE;
}

gboolean
coil_error_matches(int code)
{
    if (coil_error_occurred()) {
        return g_error_matches(_coil_error, COIL_ERROR, code);
    }
    return FALSE;
}

void
coil_error_clear(void)
{
    if (coil_error_occurred()) {
        g_error_free(_coil_error);
        _coil_error = NULL;
    }
}

GError *
coil_error_new_valist(int code, CoilLocation *location,
        const char *format, va_list args)
{
    g_return_val_if_fail(format, NULL);

    GError *error;

    if (location) {
        char *prefix, *buf;

        prefix = get_location_prefix(location);
        buf = g_strconcat(prefix, format, NULL);

        error = g_error_new_valist(COIL_ERROR, code, buf, args);

        g_free(prefix);
        g_free(buf);
    }
    else {
        error = g_error_new_valist(COIL_ERROR, code, format, args);
    }
    return error;
}

GError *
coil_error_new(int code, CoilLocation *location,
        const char *format, ...)
{
    GError *error;
    va_list args;

    va_start(args, format);
    error = coil_error_new_valist(code, location, format, args);
    va_end(args);

    return error;
}

void
coil_set_error(int code, CoilLocation *location, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    _coil_error = coil_error_new_valist(code, location, format, args);
    va_end(args);
}

void
coil_set_error_valist(int code, CoilLocation *location, const char *format,
        va_list args)
{
    _coil_error = coil_error_new_valist(code, location, format, args);
}

void
coil_set_error_literal(int code, CoilLocation *location, const char *msg)
{
    coil_set_error(code, location, msg, NULL);
}

void
coil_object_error_valist(int code, CoilObject *object, const char *format,
        va_list args)
{
    char *buf, *path;
    const char *type;

    type = G_OBJECT_TYPE_NAME(object);
    if (object->path)
        path = get_short_path(object->path);
    else
        path = get_short_path(object->container->path);

    if (path && *path)
        buf = g_strdup_printf("%s<%s>: %s", type, path, format);
    else
        buf = g_strdup_printf("%s: %s", type, format);

    coil_set_error_valist(code, &object->location, buf, args);

    g_free(path);
    g_free(buf);
}

void
coil_object_error(int code, CoilObject *object, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    coil_object_error_valist(code, object, format, args);
    va_end(args);
}

void
coil_struct_error(CoilObject *self, const char *format, ...)
{
    g_return_if_fail(self);
    g_return_if_fail(self->path);
    g_return_if_fail(format);

    va_list args;

    va_start(args, format);
    coil_object_error_valist(COIL_ERROR_STRUCT, self, format, args);
    va_end(args);
}

void
coil_path_error(const gchar *path, guint len, const char *format, ...)
{
    g_return_if_fail(path);
    g_return_if_fail(format);

    va_list args;
    const char *type;
    char *buf, *short_path;

    type = g_type_name(COIL_TYPE_PATH);
    short_path = get_short_path_string(path, len);
    buf = g_strdup_printf("%s<%s>: %s", type, short_path, format);

    va_start(args, format);
    coil_set_error_valist(COIL_ERROR_PATH, NULL, buf, args);
    va_end(args);

    g_free(short_path);
    g_free(buf);
}

