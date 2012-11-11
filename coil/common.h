/*
 * Copyright (C) 2009, 2010, 2011
 *
 * Author: John O'Connor
 */
#ifndef _COIL_COMMON_H
#define _COIL_COMMON_H

#ifndef _GNU_SOURCE
#  define _GNU_SOURCE
#endif

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#if COIL_DEBUG
#  define G_DISABLE_CHECKS 1
#endif

#include "glib.h"
#include "glib-object.h"
#include "error.h"

#include <string.h>
#include "strings_extra.h"

#define COIL_API(rtype) rtype
#define COIL_STATIC_STRLEN(str) str,(sizeof(str)-1)

#define COIL_TYPE_BOOLEAN G_TYPE_BOOLEAN
#define COIL_TYPE_INT G_TYPE_INT
#define COIL_TYPE_UINT G_TYPE_UINT
#define COIL_TYPE_LONG G_TYPE_LONG
#define COIL_TYPE_ULONG G_TYPE_ULONG
#define COIL_TYPE_INT64 G_TYPE_INT64
#define COIL_TYPE_UINT64 G_TYPE_UINT64
#define COIL_TYPE_FLOAT G_TYPE_DOUBLE /* XXX */
#define COIL_TYPE_DOUBLE G_TYPE_DOUBLE
#define COIL_TYPE_STRING G_TYPE_STRING


G_BEGIN_DECLS

void
coil_init(void);

const char *
coil_type_name(GType type);

gboolean
coil_get(CoilObject *o, const char *path, int type, gpointer return_value);

gboolean
coil_set(CoilObject *o, const char *path, int type, gpointer *value_ptr);

G_END_DECLS

#endif
