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

#define CLEAR(p, destroy_func, args...)     \
    if ((p) != NULL) {                      \
        destroy_func(p, ##args);            \
        p = NULL;                           \
    }

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
