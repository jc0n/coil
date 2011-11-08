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
  guint first_line;
  guint first_column;
  guint last_line;
  guint last_column;
  gchar *filepath;
} CoilLocation;

#include "struct.h"
#include "object.h"
#include "link.h"

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

#define coil_error_matches(err, code)                             \
        g_error_matches(err, COIL_ERROR, (code))

#define coil_path_error(err, _path, format, args...) \
        G_STMT_START { \
        g_set_error(error, \
                    COIL_ERROR, \
                    COIL_ERROR_PATH, \
                    "path error (%.*s): " format, \
                    _path->path_len, _path->path, ##args); \
        } G_STMT_END

#define coil_propagate_error g_propagate_error
#define coil_propagate_prefixed_error g_propagate_prefixed_error

G_BEGIN_DECLS

GQuark
coil_error_quark(void);

GType
coil_location_get_type(void) G_GNUC_CONST;

GError *
coil_error_new_valist(int code, CoilLocation *location,
                      const char *format, va_list args);

GError *
coil_error_new(int code, CoilLocation *location,
               const char *format, ...);


void
coil_set_error(GError **error, int code,
               CoilLocation *location,
               const char *format, ...);

void
coil_set_error_valist(GError **error, int code,
                      CoilLocation *location, const char *format,
                      va_list args);

void
coil_set_error_literal(GError **error, int code, CoilLocation *location,
                       const char *message);

void
coil_object_error(GError **error, int code, gconstpointer obj,
                      const char *format,
                      ...);

void
coil_struct_error(GError **error, const CoilStruct *obj,
                  const char *format, ...);

void
coil_link_error(GError **error, const CoilLink *obj,
                const char *format, ...);

G_END_DECLS

#endif

