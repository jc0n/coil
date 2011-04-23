/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */
#ifndef _COIL_ERROR_H
#define _COIL_ERROR_H

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

typedef struct _CoilLocation
{
  guint first_line;
  guint first_column;
  guint last_line;
  guint last_column;
  gchar *filepath;
} CoilLocation;

#define coil_error_occured(e) (G_UNLIKELY((e) != NULL))

#define COIL_TYPE_LOCATION (coil_location_get_type())

#define COIL_LOCATION_FORMAT "line %d in file %s "
#define COIL_LOCATION_FORMAT_ARGS(loc)                            \
        (loc).first_line, (loc).filepath
/*
        (loc).first_line, (loc).first_column,
        (loc).last_line, (loc).last_column
*/

#define _COIL_NOT_IMPLEMENTED_ACTION                              \
    g_error("%s:%s Not implemented.",                             \
            G_STRFUNC,                                            \
            G_STRLOC);                                            \
    g_assert_not_reached();                                       \

#define COIL_NOT_IMPLEMENTED(rtype)                               \
  G_STMT_START                                                    \
  {                                                               \
    _COIL_NOT_IMPLEMENTED_ACTION                                  \
    return (rtype);                                               \
  }                                                               \
  G_STMT_END

#define COIL_NOT_IMPLEMENTED_VOID                                 \
  G_STMT_START                                                    \
  {                                                               \
    _COIL_NOT_IMPLEMENTED_ACTION                                  \
  }                                                               \
  G_STMT_END

#define COIL_ERROR coil_error_quark()

#define coil_error_new(code, location, format, args...)           \
        g_error_new(COIL_ERROR,                                   \
                    (code),                                       \
                    COIL_LOCATION_FORMAT format,                  \
                    COIL_LOCATION_FORMAT_ARGS(location),          \
                    ##args)

#define coil_error_new_literal(code, location, message)           \
        g_error_new(COIL_ERROR,                                   \
                    (code),                                       \
                    COIL_LOCATION_FORMAT "%s",                    \
                    COIL_LOCATION_FORMAT_ARGS(location),          \
                    message)

#define coil_set_error(err, code, location, format, args...)      \
        g_set_error((err),                                        \
                    COIL_ERROR,                                   \
                    (code),                                       \
                    COIL_LOCATION_FORMAT format,                  \
                    COIL_LOCATION_FORMAT_ARGS(location),          \
                    ##args)

#define coil_set_error_literal(err, code, location, message)      \
        g_set_error_literal((err),                                \
                            COIL_ERROR,                           \
                            (code),                               \
                            COIL_LOCATION_FORMAT "%s",            \
                            message)

#define coil_expandable_error(err, code, ex, format, args...) \
        coil_set_error(err, code, \
                       COIL_EXPANDABLE(ex)->location, \
                       format, \
                       ##args)
/*
#define coil_struct_error(err, st, format, args...) \
        coil_expandable_set_error(err, COIL_ERROR_STRUCT, \
                        st, "(in struct %s) " format, \
                        coil_struct_get_path(st)->path, \
                        ##args)
                        */

#define coil_struct_error(err, node, format, args...) \
  coil_expandable_error(err, COIL_ERROR_STRUCT, \
                        node, "<%s>: " format, \
                        coil_struct_get_path(node)->path, \
                        ##args)

#define coil_link_error(err, link, format, args...) \
        coil_expandable_error(err, COIL_ERROR_LINK, \
                        link, "<%s>: " format, \
                        coil_link_get_path(link)->path, \
                        ##args)

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

extern GError **coil_error;

GQuark
coil_error_quark(void);

GType
coil_location_get_type(void) G_GNUC_CONST;

G_END_DECLS

#endif

