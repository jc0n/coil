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
#ifndef _COIL_INCLUDE_H
#define _COIL_INCLUDE_H

#include "struct.h"

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

/* XXX: make private */
#define coil_include_new(args...)                                             \
    COIL_OBJECT(g_object_new(COIL_TYPE_INCLUDE, ##args, NULL))

#define coil_include_error(incl, format, args...) \
    coil_object_error(COIL_ERROR_INCLUDE, incl, format, ##args)

typedef struct _CoilInclude         CoilInclude;
typedef struct _CoilIncludeClass    CoilIncludeClass;
typedef struct _CoilIncludePrivate  CoilIncludePrivate;

struct _CoilInclude
{
    CoilObject      parent_instance;
    CoilIncludePrivate *priv;
};

struct _CoilIncludeClass
{
    CoilObjectClass parent_class;
};

G_BEGIN_DECLS

GType
coil_include_get_type(void) G_GNUC_CONST;

/* XXX: remove (replace with property after errors are refactorerd) */
CoilObject *
coil_include_get_root_node(CoilObject *include);

CoilObject *
coil_include_dup_root_node(CoilObject *include);

G_END_DECLS

#endif
