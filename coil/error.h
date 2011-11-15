/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */
#ifndef _COIL_ERROR_H
#define _COIL_ERROR_H

#define COIL_TYPE_LOCATION (coil_location_get_type())

typedef struct _CoilLocation
{
    guint line;
    gchar *filepath;
} CoilLocation;

#include "object.h"

typedef enum
{
    COIL_ERROR_INCLUDE,
    COIL_ERROR_INTERNAL,
    COIL_ERROR_KEY,
    COIL_ERROR_KEY_MISSING,
    COIL_ERROR_LINK,
    COIL_ERROR_PARSE,
    COIL_ERROR_PATH,
    COIL_ERROR_STRUCT,
    COIL_ERROR_VALUE,
} CoilError;

#define COIL_ERROR coil_error_quark()
#define CoilError GError
#define coil_error_free g_error_free
#define coil_error_copy g_error_copy

G_BEGIN_DECLS

GQuark
coil_error_quark(void);

GType
coil_location_get_type(void) G_GNUC_CONST;

gboolean
coil_error_occurred(void);

gboolean
coil_get_error(CoilError **error);

gboolean
coil_error_matches(int code);

void
coil_error_clear(void);

GError *
coil_error_new_valist(int code, CoilLocation *location, const char *format,
        va_list args);

GError *
coil_error_new(int code, CoilLocation *location, const char *format, ...);

void
coil_set_error(int code, CoilLocation *location, const char *format, ...);

void
coil_set_error_valist(int code, CoilLocation *location, const char *format,
        va_list args);

void
coil_set_error_literal(int code, CoilLocation *location, const char *msg);

void
coil_object_error(int code, CoilObject *object, const char *format, ...);

void
coil_struct_error(CoilObject *self, const char *format, ...);

void
coil_link_error(CoilObject *self, const char *format, ...);

void
coil_path_error(const gchar *str, guint len, const char *format, ...);

G_END_DECLS
#endif

