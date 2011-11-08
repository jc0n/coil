/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */

#include "coil.h"

/*
 * coil_error_quark:
 *
 * Return error identifier for Glib Error Handling
 */
GQuark coil_error_quark(void)
{
  static GQuark result = 0;

  if (!result)
    result = g_quark_from_static_string("coil-error-quark");

  return result;
}


static char *
location_prefix(CoilLocation *location)
{
    int line = location->first_line;
    const char *filepath = location->filepath;

    if (line > 0 && filepath != NULL)
        return g_strdup_printf("line %d in file %s", line, filepath);
    else if (line > 0)
        return g_strdup_printf("line %d", line);
    else if (filepath != NULL)
        return g_strdup_printf("file %s", filepath);

    return g_strndup("", 0);
}

GError *
coil_error_new_valist(int code, CoilLocation *location,
                      const char *format, va_list args)
{
    g_return_val_if_fail(location != NULL, NULL);
    g_return_val_if_fail(format != NULL, NULL);

    GError *err;
    char *pfx, *real_format;

    pfx = location_prefix(location);
    real_format = g_strconcat(pfx, format, NULL);
    g_free(pfx);

    err = g_error_new(COIL_ERROR, code, real_format, args);

    g_free(real_format);

    return err;
}

GError *
coil_error_new(int code, CoilLocation *location,
               const char *format, ...)
{
    GError *err;
    va_list args;

    va_start(args, format);
    err = coil_error_new_valist(code, location, format, args);
    va_end(args);

    return err;
}


void
coil_set_error(GError **error, int code,
               CoilLocation *location,
               const char *format, ...)
{
    GError *new;
    va_list args;

    va_start(args, format);
    new = coil_error_new_valist(code, location, format, args);
    va_end(args);

    g_propagate_error(error, new);
}

void
coil_set_error_valist(GError **error, int code,
                      CoilLocation *location, const char *format,
                      va_list args)
{
    GError *new = coil_error_new_valist(code, location, format, args);
    g_propagate_error(error, new);
}

void
coil_set_error_literal(GError **error, int code, CoilLocation *location,
                       const char *message)
{
    coil_set_error(error, code, location, message, NULL);
}

void
coil_object_error(GError **error, int code, gconstpointer obj,
                      const char *format,
                      ...)
{
    va_list args;
    CoilLocation *loc;
    CoilObject *ex = COIL_OBJECT(obj);

    loc = &ex->location;
    va_start(args, format);
    coil_set_error_valist(error, code, loc, format, args);
    va_end(args);
}


void
coil_struct_error(GError **error, const CoilStruct *obj,
                  const char *format, ...)
{
    char *real_format, *path;
    va_list args;
    CoilLocation *loc;

    path = coil_struct_get_path(obj)->path;
    loc = &COIL_OBJECT(obj)->location;
    real_format = g_strdup_printf("<%s>: %s", path, format);

    va_start(args, format);
    coil_set_error_valist(error, COIL_ERROR_STRUCT, loc, real_format, args);
    va_end(args);

    g_free(real_format);
}

void
coil_link_error(GError **error, const CoilLink *obj,
                const char *format, ...)
{
    char *real_format;
    const CoilPath *path;
    CoilLocation *lo;
    va_list args;

    path = coil_link_get_path(obj);
    lo = &COIL_OBJECT(obj)->location;

    va_start(args, format);

    if (path) {
        real_format = g_strdup_printf("Link<%s>: %s", path->path, format);
        coil_set_error_valist(error, COIL_ERROR_LINK, lo, real_format, args);
        g_free(real_format);
    }
    else {
        coil_set_error_valist(error, COIL_ERROR_LINK, lo, format, args);
    }

    va_end(args);
}


