/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */
#ifndef _COIL_INCLUDE_H
#define _COIL_INCLUDE_H

#include "struct.h"

#define MAX_INCLUDE_DEPTH 1 << 8

#define COIL_TYPE_INCLUDE                                                     \
        (coil_include_get_type())

#define COIL_INCLUDE(obj)                                                     \
        (G_TYPE_CHECK_INSTANCE_CAST((obj), COIL_TYPE_INCLUDE,                 \
         CoilInclude))

#define COIL_IS_INCLUDE(obj)                                                  \
        (G_TYPE_CHECK_INSTANCE_TYPE((obj), COIL_TYPE_INCLUDE))

#define COIL_INCLUDE_CLASS(klass)                                             \
        (G_TYPE_CHECK_CLASS_CAST((klass), COIL_TYPE_INCLUDE,                  \
         CoilIncludeClass))

#define COIL_IS_INCLUDE_CLASS(klass)                                          \
        (G_TYPE_CHECK_CLASS_TYPE((klass), COIL_TYPE_INCLUDE)

#define COIL_INCLUDE_GET_CLASS(obj)                                           \
        (G_TYPE_INSTANCE_GET_CLASS((obj), COIL_TYPE_INCLUDE,                  \
         CoilIncludeClass))

#define coil_include_new(args...)                                             \
    g_object_new(COIL_TYPE_INCLUDE, ##args, NULL)

#define coil_include_error(err, incl, format, args...) \
    coil_expandable_error(err, COIL_ERROR_INCLUDE, \
                          incl, "Include error: " format, ##args)

typedef struct _CoilInclude         CoilInclude;
typedef struct _CoilIncludeClass    CoilIncludeClass;
typedef struct _CoilIncludePrivate  CoilIncludePrivate;

struct _CoilInclude
{
  CoilExpandable      parent_instance;
  CoilIncludePrivate *priv;
};

struct _CoilIncludeClass
{
  CoilExpandableClass parent_class;
};

G_BEGIN_DECLS

GType
coil_include_get_type(void) G_GNUC_CONST;


gboolean
coil_include_equals(gconstpointer   e1,
                    gconstpointer   e2,
                    GError        **error);

void
coil_include_build_string(CoilInclude *self,
                          GString     *const buffer,
                          CoilStringFormat *format,
                          GError     **error);

gchar *
coil_include_to_string(CoilInclude      *self,
                       CoilStringFormat *format,
                       GError          **error);

G_END_DECLS

#endif
